// SELF
#include "CMT.h"
#include "gui.h"
#include "ros_grabber.hpp"

// CV
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/utility.hpp>

// STD
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>

#ifdef __GNUC__
#include <getopt.h>
#else
#include "getopt/getopt.h"
#endif

using cmt::CMT;
using cv::imread;
using cv::namedWindow;
using cv::Scalar;
using cv::VideoCapture;
using cv::waitKey;
using std::cerr;
using std::istream;
using std::ifstream;
using std::stringstream;
using std::ofstream;
using std::cout;
using std::min_element;
using std::max_element;
using std::endl;
using ::atof;

static string WIN_NAME = "CMT";

string topic = "/usb_cam/image_raw";
bool pyr = false;
float UPPER_I = 0;


int display(Mat im, CMT & cmt, int result)
{
    if(result > UPPER_I-(UPPER_I/100)*10 && result <= UPPER_I+(UPPER_I/100)*10) {
        for(size_t i = 0; i < cmt.points_active.size(); i++)
        {
            circle(im, cmt.points_active[i], 2, Scalar(255,0,0));
        }

        Point2f vertices[4];
        cmt.bb_rot.points(vertices);
        for (int i = 0; i < 4; i++)
        {
            line(im, vertices[i], vertices[(i+1)%4], Scalar(255,0,0));
        }
    }

    imshow(WIN_NAME, im);
    return waitKey(5);
}

int main(int argc, char **argv)
{
    // ROS
    // FILELog::ReportingLevel() = logDEBUG;
    FILELog::ReportingLevel() = logINFO;

    ros::init(argc, argv, "clf_cmt", ros::init_options::AnonymousName);

    //Create a CMT object
    CMT cmt;

    //Parse args
    int challenge_flag = 0;
    int loop_flag = 0;

    const int detector_cmd = 1000;
    const int descriptor_cmd = 1001;
    const int bbox_cmd = 1002;
    const int no_scale_cmd = 1003;
    const int with_rotation_cmd = 1004;
    const int skip_cmd = 1005;
    const int skip_msecs_cmd = 1006;
    const int output_file_cmd = 1007;
    unsigned int last_computed_frame = -1;

    //Create window
    namedWindow(WIN_NAME);

    bool show_preview = true;

    cv::CommandLineParser parser(argc, argv, "{@config |<none>| yaml config file}" "{help h ||}");
    cv::FileStorage fs(argv[1], cv::FileStorage::READ);

    if (fs.isOpened()) {

        fs["input_ros_topic"] >> topic;
        cout << ">>> Input Topic: --> " << topic << endl;

        fs["pyr"] >> pyr;
        cout << ">>> PyrUP: --> " << pyr << endl;
    }

    fs.release();

    // ROS
    ROSGrabber ros_grabber(topic);
    ros_grabber.setPyr(pyr);

    //Initialize CMT
    cv::Rect rect;
    rect.x = 320-50;
    rect.y = 240-50;
    rect.width = 50;
    rect.height = 50;

    Mat im0, im0_gray;

    while (im0.empty()) {
        ros::spinOnce();
        ros_grabber.getImage(&im0);
    }

    cvtColor(im0, im0_gray, CV_BGR2GRAY);
    cmt.initialize(im0_gray, rect);
    UPPER_I = (float) cmt.points_active.size();

    //The image
    Mat im;
    float result = 0;

    //Main loop
    while (true)
    {
        ros::spinOnce();
        ros_grabber.getImage(&im);
        int tmp_frame_nr = ros_grabber.getLastFrameNr();
        if(last_computed_frame != tmp_frame_nr) {
            if (im.empty()) continue; //Exit at end of video stream
            Mat im_gray;
            if (im.channels() > 1) {
                cvtColor(im, im_gray, CV_BGR2GRAY);
            } else {
                im_gray = im;
            }
            // Let CMT process the frame
            cmt.processFrame(im_gray);
            result = (float)cmt.points_active.size();
        }
        last_computed_frame = ros_grabber.getLastFrameNr();
        // Display image and then quit if requested.
        char key = display(im, cmt, result);
        if(key == 'q') break;
    }

    return 0;
}