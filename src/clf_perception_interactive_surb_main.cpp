/*

Author: Florian Lier [flier AT techfak.uni-bielefeld DOT de]

By downloading, copying, installing or using the software you agree to this license.
If you do not agree to this license, do not download, install, copy or use the software.

                          License Agreement
               For Open Source Computer Vision Library
                       (3-clause BSD License)

Copyright (C) 2000-2016, Intel Corporation, all rights reserved.
Copyright (C) 2009-2011, Willow Garage Inc., all rights reserved.
Copyright (C) 2009-2016, NVIDIA Corporation, all rights reserved.
Copyright (C) 2010-2013, Advanced Micro Devices, Inc., all rights reserved.
Copyright (C) 2015-2016, OpenCV Foundation, all rights reserved.
Copyright (C) 2015-2016, Itseez Inc., all rights reserved.
Third party copyrights are property of their respective owners.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  * Neither the names of the copyright holders nor the names of the contributors
    may be used to endorse or promote products derived from this software
    without specific prior written permission.

This software is provided by the copyright holders and contributors "as is" and
any express or implied warranties, including, but not limited to, the implied
warranties of merchantability and fitness for a particular purpose are disclaimed.
In no event shall copyright holders or contributors be liable for any direct,
indirect, incidental, special, exemplary, or consequential damages
(including, but not limited to, procurement of substitute goods or services;
loss of use, data, or profits; or business interruption) however caused
and on any theory of liability, whether in contract, strict liability,
or tort (including negligence or otherwise) arising in any way out of
the use of this software, even if advised of the possibility of such damage.

*/


// ROS
#include <ros/ros.h>
#include <std_msgs/Bool.h>

// SELF
#include "ros_grabber.hpp"
#include "clf_perception_interactive_surb.hpp"

using namespace std;

bool toggle;
double time_spend;
unsigned int frame_count;
unsigned int average_frames;

void toggle_callback(const std_msgs::Bool& _toggle) {
    toggle = _toggle.data;
    cout << ">>> I am currently computing? --> " << toggle << endl;
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        cout << ">>> Usage: <proc> {path/to/config/file}" << endl;
        return -1;
    }
    
    toggle = true;
    frame_count = 0;
    time_spend = 0;
    average_frames = 0;
    time_t start, end;

    ros::init(argc, argv, "clf_perception_interactive_surb", ros::init_options::AnonymousName);

    // How many CPUs do we have?
    cout << ">>> Found --> " << cv::getNumberOfCPUs() << " CPUs"<< endl;

    // Are we using optimized OpenCV Code?
    cout << ">>> OpenCV was built with optimizations --> " << cv::useOptimized() << endl;

    Detect2DInteractive detect2d;
    detect2d.setup(argc, argv);

    ROSGrabber ros_grabber(detect2d.ros_input_topic);

    ros::Subscriber sub = ros_grabber.node_handle_.subscribe("/clf_perception_interactive_surb/compute", 1, toggle_callback);
    ros::ServiceServer service = ros_grabber.node_handle_.advertiseService("clf_interactive_surb_learn", &Detect2DInteractive::addTarget, &detect2d);

    cout << ">>> Ready, let's go..." << endl;

    cv::namedWindow("CLF PERCEPTION || Surb", cv::WINDOW_AUTOSIZE + cv::WINDOW_OPENGL);
    cv::Mat current_image;

    cout << ">>> Press 'ESC' to exit" << endl;

    int last_computed_frame = 0;
    time(&start);

    while(cv::waitKey(5) != 27) {

        ros::spinOnce();

        boost::posix_time::ptime start_main = boost::posix_time::microsec_clock::local_time();

        if(toggle) {
            try {
                ros_grabber.getImage(&current_image);
                if (current_image.rows*current_image.cols > 0) {
                    int tmp_frame_nr = ros_grabber.getLastFrameNr();
                    if(last_computed_frame != tmp_frame_nr) {
                        detect2d.detect(current_image, ros_grabber.getTimestamp(), ros_grabber.frame_id);
                        last_computed_frame = ros_grabber.getLastFrameNr();
                        frame_count++;
                    } else {
                        continue;
                    }
                } else {
                    // cout << "E >>> Image could not be grabbed" << endl;
                    continue;
                }
            } catch (std::exception& e) {
                cout << "E >>> " << e.what() << endl;
            }

            boost::posix_time::ptime end_main = boost::posix_time::microsec_clock::local_time();
            boost::posix_time::time_duration diff_main = end_main - start_main;
            string string_time_main = to_string(diff_main.total_milliseconds());

            if (!detect2d.get_silent()) {

                cv::rectangle(current_image, cv::Point2d(current_image.cols-160, 88),
                              cv::Point2d(current_image.cols, 102), CV_RGB(128,128,128), CV_FILLED);

                cv::rectangle(current_image, cv::Point2d(current_image.cols-160, 108),
                              cv::Point2d(current_image.cols, 122), CV_RGB(128,128,128), CV_FILLED);


                cv::putText(current_image, "Total: "+string_time_main+" ms", cv::Point2d(current_image.cols-160, 100),
                            detect2d.fontFace, detect2d.fontScale, cv::Scalar(235, 206, 135), 1);


                cv::putText(current_image, "FPS: "+to_string(average_frames)+" ", cv::Point2d(current_image.cols-160, 120),
                            detect2d.fontFace, detect2d.fontScale, cv::Scalar(235, 206, 135), 1.2);

                if (current_image.cols > 1300) {
                    cv::Size size(current_image.cols/1.5,current_image.rows/1.5);
                    cv::Mat resize;
                    cv::resize(current_image, resize, size, cv::INTER_NEAREST);
                    cv::imshow("CLF PERCEPTION || Surb", resize);
                } else {
                    cv::imshow("CLF PERCEPTION || Surb", current_image);
                }

                if (time_spend >= 1 ) {
                    average_frames = frame_count;
                    time(&start);
                    frame_count = 0;
                }

                time(&end);
                time_spend = difftime(end, start);
            }
        }
    }

    ros::shutdown();

    cout << ">>> Cleaning Up. Goodbye!" << endl;

    return 0;
}

