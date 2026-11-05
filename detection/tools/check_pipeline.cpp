#include "cli.h"
#include "calibration.h"
#include "preprocess.h"
#include "coordinate_restore.h"
#include "armor_geometry.h"
#include "pose.h"
#include "camera_transform.h"
#include "json_output.h"
#include <opencv2/calib3d.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
using nlohmann::json;
namespace
{
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void verify_detection(const json& d, const CameraCalibration& camera)
{
    require(d.size() == 6, "Unexpected detection fields");
    const std::vector<std::string> colors{"blue","red","neutral","purple"};
    const std::vector<std::string> patterns{"sentry","1","2","3","4","5","outpost","base_small","base_big"};
    require(std::find(colors.begin(),colors.end(),d.at("color")) != colors.end(), "Bad color");
    require(std::find(patterns.begin(),patterns.end(),d.at("pattern")) != patterns.end(), "Bad pattern");
    const double confidence = d.at("confidence");
    require(std::isfinite(confidence) && confidence >= 0 && confidence <= 1, "Bad confidence");
    require(d.at("corners_px").size() == 4, "Bad 2D corner count");
    for (const auto& p : d.at("corners_px"))
        require(p.size() == 2 && std::isfinite(p[0].get<double>()) && std::isfinite(p[1].get<double>()), "Bad 2D point");
    if (d.at("corners_camera_m").is_null())
    {
        require(d.at("reprojection_error_px").is_null(), "Failure fields must both be null");
        return;
    }
    require(d.at("corners_camera_m").size() == 4, "Bad 3D corner count");
    std::vector<cv::Point3d> points;
    for (const auto& p : d.at("corners_camera_m"))
    {
        require(p.size()==3, "Bad 3D dimensions");
        cv::Point3d q(p[0].get<double>(),p[1].get<double>(),p[2].get<double>());
        require(std::isfinite(q.x)&&std::isfinite(q.y)&&std::isfinite(q.z)&&q.z>0, "Bad 3D point");
        points.push_back(q);
    }

    const double width = (d.at("pattern")=="1" || d.at("pattern")=="base_big") ? 0.230 : 0.135;
    require(std::abs(cv::norm(points[1]-points[0])-0.056)<1e-8, "Wrong physical height");
    require(std::abs(cv::norm(points[2]-points[1])-width)<1e-8, "Wrong physical width");
    std::vector<cv::Point2d> projected;
    cv::projectPoints(points, cv::Vec3d(0,0,0), cv::Vec3d(0,0,0),
                      camera.camera_matrix,camera.dist_coeffs,projected);
    double squared=0;
    for(int k=0;k<4;++k)
    {
        const double dx=projected[k].x-d["corners_px"][k][0].get<double>();
        const double dy=projected[k].y-d["corners_px"][k][1].get<double>();
        squared+=dx*dx+dy*dy;
    }
    const double rms=d.at("reprojection_error_px");
    require(std::isfinite(rms)&&rms>=0&&std::abs(rms-std::sqrt(squared/4))<1e-6, "Incorrect RMS");
}
void verify_fixture(const std::string& path,const CameraCalibration& camera)
{
    std::ifstream input(path);
    require(input.is_open(), "Cannot read fixture output");
    std::string line;
    int index=0, count=0;
    while(std::getline(input,line))
    {
        const auto frame=json::parse(line);
        require(frame.size()==3 && frame.at("frame_index")==index, "Missing/out-of-order frame");
        require(std::abs(frame.at("timestamp_s").get<double>()-index/30.0)<1e-12,"Wrong timestamp");
        require(frame.at("detections").is_array(),"Detections must be array");
        for(const auto& d:frame["detections"]) { verify_detection(d,camera); ++count; }
        ++index;
    }
    require(index==360,"Fixture must contain 360 lines");
    std::cout<<path<<": 360 frames, "<<count<<" detections, schema/geometry/reprojection OK\n";
}
}
int main(int argc,char** argv)
{
    try
    {
        const auto camera=load_calibration(std::string(VISION_ASSET_DIR)+"/calibration.yaml");

        auto input=preprocess(cv::Mat(2,4,CV_8UC3,cv::Scalar(10,20,30)));
        require(input.scale==160 && input.pad_top==160 && input.pad_left==0,"Wrong letterbox");
        const int offset=160*640;
        require(std::abs(input.data[offset].ToFloat()-30.0f/255)<0.001,"Wrong red channel");
        require(std::abs(input.data[640*640+offset].ToFloat()-20.0f/255)<0.001,"Wrong green channel");
        require(std::abs(input.data[2*640*640+offset].ToFloat()-10.0f/255)<0.001,"Wrong blue channel");
        require(std::abs(input.data[0].ToFloat()-114.0f/255)<0.001,"Wrong padding value");

        std::vector<float> raw(25200*22,0);
        require(postprocess(raw).empty(),"Expected empty detections");
        auto row=[&](int i,float x,float score) {
            float* p=raw.data()+i*22;
            const float points[]{x,10,x,30,x+40,30,x+40,10};
            std::copy(points,points+8,p); p[8]=score; p[10]=3; p[17]=4;
        };
        row(0,100,-3); require(postprocess(raw).empty(),"Coordinate used as confidence");
        row(1,100,3);row(2,101,2);row(25199,300,2);
        const auto decoded=postprocess(raw);
        require(decoded.size()==2 && decoded[0].color_id==1 && decoded[0].pattern_id==4,"NMS/decode failure");

        std::vector<Detection> detections(1);
        detections[0].corners.fill(cv::Point2f(320,320));
        input.scale=4.0/9; input.pad_left=0;input.pad_top=80;
        restore_coordinates(detections,input);
        require(cv::norm(detections[0].corners[0]-cv::Point2f(720,540))<1e-5,"Inverse transform failure");

        const cv::Vec3d r(0.2,-0.3,0.1),t(0.1,-0.05,2);
        cv::Mat rotation;cv::Rodrigues(r,rotation);
        for(int id=0;id<9;++id)
        {
            const double w=(id==1||id==8)?0.230:0.135;
            const std::vector<cv::Point3d> objects{{-w/2,-0.028,0},{-w/2,0.028,0},{w/2,0.028,0},{w/2,-0.028,0}};
            std::vector<cv::Point2d> pixels;
            cv::projectPoints(objects,r,t,camera.camera_matrix,camera.dist_coeffs,pixels);
            Detection d{};d.pattern_id=id;d.color_id=id%4;d.confidence=0.8f;
            for(int k=0;k<4;++k)d.corners[k]=pixels[k];
            auto pose=estimate_pose(d,camera);
            auto transformed=transform_corners(id,pose);
            require(transformed.has_value(),"Synthetic pose failed");
            for(int k=0;k<4;++k)
            {
                cv::Vec3d expected;
                for(int a=0;a<3;++a)expected[a]=rotation.at<double>(a,0)*objects[k].x+rotation.at<double>(a,1)*objects[k].y+t[a];
                require(cv::norm(transformed->corners[k]-cv::Point3d(expected))<1e-3,"Incorrect R*Po+t");
            }

            std::ostringstream stream;
            write_frame_json(stream,0,{{d,transformed}});
            auto frame=json::parse(stream.str());verify_detection(frame["detections"][0],camera);
            require(frame["detections"][0]["pattern"].is_string(),"Pattern must be string");
            std::ostringstream failed;write_frame_json(failed,1,{{d,std::nullopt}});
            auto failure=json::parse(failed.str());
            require(failure["detections"].size()==1&&failure["detections"][0]["corners_camera_m"].is_null()&&failure["detections"][0]["reprojection_error_px"].is_null(),"Lost failed detection");
            d.corners[0].x=std::numeric_limits<float>::quiet_NaN();
            require(!estimate_pose(d,camera),"NaN input accepted");
        }
        std::ostringstream empty;write_frame_json(empty,0,{});
        require(json::parse(empty.str())["detections"]==json::array(),"Empty frame omitted");
        std::ostringstream broken;broken.setstate(std::ios::badbit);
        bool rejected=false;try{write_frame_json(broken,0,{});}catch(const std::exception&){rejected=true;}
        require(rejected,"Write failure ignored");
        require(!transform_corners(1,std::nullopt),"Missing pose accepted");

        ProgramOptions config;config.video=std::string(VISION_ASSET_DIR)+"/small.mp4";config.output=config.video;
        rejected=false;try{validate_output_path(config);}catch(const std::exception&){rejected=true;}
        require(rejected,"Input overwrite allowed");
        std::cout<<"Synthetic checks for steps 0-6 passed\n";
        for(int i=1;i<argc;++i)verify_fixture(argv[i],camera);
        return 0;
    }
    catch(const std::exception& error){std::cerr<<"Pipeline check failed: "<<error.what()<<'\n';return 1;}
}
