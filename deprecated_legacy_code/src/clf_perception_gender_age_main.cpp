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


// SELF
#include "ros_grabber.hpp"
#include "dlib_detection.hpp"

using namespace std;
using namespace cv;

string topic,
       shape_mode_path,
       model_file_age,
       model_file_gender,
       trained_file_age,
       trained_file_gender,
       mean_file,
       label_file_age,
       label_file_gender;

bool toggle = true;
unsigned int _pyr = 0;
unsigned int gender_age = 1;
unsigned int frame_count = 0;
unsigned int average_frames = 0;
double time_spend = 0;
unsigned int microseconds = 1000;
const int fontFace = cv::FONT_HERSHEY_PLAIN;
const double fontScale = 1;

void toggle_callback(const std_msgs::Bool& _toggle) {
    toggle = _toggle.data;
    cout << ">>> I am currently computing? --> " << toggle << endl;
}

cv::Rect dlib2cvrect(const dlib::rectangle& r) {return cv::Rect(r.left(), r.top(), r.width(), r.height());}

int main(int argc, char *argv[]) {

    time_t start, end;

    if (argc < 1) {
        cout << ">>> Usage: <proc> {path/to/config/file}" << endl;
        cout << ">>> Example: <proc> /tmp/example.yaml" << endl;
        return -1;
    }

    ros::init(argc, argv, "clf_perception_gender_age", ros::init_options::AnonymousName);

    // How many CPUs do we have?
    cout << ">>> Found --> " << cv::getNumberOfCPUs() << " CPUs"<< endl;

    // Are we using optimized OpenCV Code?
    cout << ">>> OpenCV was built with optimizations --> " << cv::useOptimized() << endl;

    CommandLineParser parser(argc, argv, "{@config |<none>| yaml config file}" "{help h ||}");
    FileStorage fs(argv[1], FileStorage::READ);

    if (fs.isOpened()) {

        fs["input_ros_topic"] >> topic;
        cout << ">>> Input Topic: --> " << topic << endl;

        fs["dlib_shapepredictor"] >> shape_mode_path;
        cout << ">>> Frontal Face: --> " << shape_mode_path << endl;

        _pyr = (int) fs["pyr_up"];

        if (_pyr > 0) {
            cout << ">>> Image Scaling is: --> ON" << endl;
        } else {
            cout << ">>> Image Scaling is: --> OFF" << endl;
        }

        fs["model_file_gender"] >> model_file_gender;
        cout << ">>> Caffee Model Gender: --> " << model_file_gender << endl;

        fs["model_file_age"] >> model_file_age;
        cout << ">>> Caffee Model Age: --> " << model_file_age << endl;

        fs["trained_file_gender"] >> trained_file_gender;
        cout << ">>> Caffee Trained Gender: --> " << trained_file_gender << endl;

        fs["trained_file_age"] >> trained_file_age;
        cout << ">>> Caffee Trained Age: --> " << trained_file_age << endl;

        fs["mean_file"] >> mean_file;
        cout << ">>> Caffe Mean: --> " << mean_file << endl;

        fs["label_file_gender"] >> label_file_gender;
        cout << ">>> Labels Gender: --> " << label_file_gender << endl;

        fs["label_file_age"] >> label_file_age;
        cout << ">>> Labels Age: --> " << label_file_age << endl;

    }

    fs.release();

    cout << ">>> ROS In Topic --> " << topic << endl;

    ROSGrabber ros_grabber(topic);
    ros_grabber.setPyr(_pyr);

    ros::Subscriber sub = ros_grabber.node_handle_.subscribe("/clf_perception_gender_age/compute", 1, toggle_callback);
    ros::Publisher people_pub = ros_grabber.node_handle_.advertise<people_msgs::People>("/clf_perception_gender_age/people", 1);

    DlibFace dlf;
    dlf.setup(shape_mode_path);

    // Caffee
    dlf.cl = new Classifier(model_file_gender, trained_file_gender, mean_file, label_file_gender);
    dlf.cl_age = new Classifier(model_file_age, trained_file_age, mean_file, label_file_age);

    cout << ">>> Let's go..." << endl;

    cv::namedWindow(":: CLF PERCEPTION GENDER AGE ::", cv::WINDOW_AUTOSIZE + cv::WINDOW_OPENGL);
    int last_computed_frame = 0;
    cv::Mat current_image, display_image;
    time(&start);

    while(cv::waitKey(5) != 27) {

        ros::spinOnce();
        // usleep(microseconds);

        if(toggle) {
            try {
                ros_grabber.getImage(&current_image);
                if (current_image.rows*current_image.cols > 0) {
                    int tmp_frame_nr = ros_grabber.getLastFrameNr();
                    if(last_computed_frame != tmp_frame_nr) {
                        display_image = current_image.clone();
                        std::vector<dlib::rectangle> current_faces;
                        current_faces = dlf.detect(current_image);
                        last_computed_frame = ros_grabber.getLastFrameNr();
                        for(int i = 0; i < current_faces.size(); ++i) {
                            cv::rectangle(display_image, dlib2cvrect(current_faces[i]), Scalar(0,165,255), 2);
                        }
                        if (current_faces.size() > 0) {
                             // --------------------------------------------
                             // do gender classification, if enabled and display results
                             // --------------------------------------------
                             std::vector<string> age_gen;
                             if (gender_age > 0) {
                                 for (int i = 0; i < current_faces.size(); i++)
                                 {
                                       cv::Mat cropped_image = display_image(Rect(dlib2cvrect(current_faces[i]).x,
                                                                             dlib2cvrect(current_faces[i]).y,
                                                                             dlib2cvrect(current_faces[i]).width,
                                                                             dlib2cvrect(current_faces[i]).height)).clone();
                                       std::vector<Prediction> predictions = dlf.cl->Classify(cropped_image);
                                       std::vector<Prediction> predictions_age = dlf.cl_age->Classify(cropped_image);
                                       Prediction p = predictions[0];
                                       Prediction p_age = predictions_age[0];
                                       if (p.second >= 0.7)
                                       {
                                          if (p.first == "male")
                                          {
                                             putText(display_image, p.first+p_age.first, Point(dlib2cvrect(current_faces[i]).x,
                                                                                   dlib2cvrect(current_faces[i]).y + dlib2cvrect(current_faces[i]).height + 20),
                                                                                   fontFace, fontScale, CV_RGB(70,130,180));
                                          }
                                          else if(p.first == "female")
                                          {
                                             putText(display_image, p.first+p_age.first, Point(dlib2cvrect(current_faces[i]).x,
                                                                                   dlib2cvrect(current_faces[i]).y + dlib2cvrect(current_faces[i]).height + 20),
                                                                                   fontFace, fontScale, CV_RGB(221,160,221));
                                          }
                                          age_gen.push_back(p.first+":"+p_age.first);
                                       } else {
                                          age_gen.push_back("unknown:unknown");
                                       }
                                 }
                             }
                             std_msgs::Header h;
                             h.stamp = ros_grabber.getTimestamp();
                             h.frame_id = ros_grabber.frame_id;
                             people_msgs::People people_msg;
                             people_msgs::Person person_msg;
                             people_msg.header = h;
                             for (int i = 0; i < current_faces.size(); ++i) {
                                if (gender_age > 0) {
                                    person_msg.name = age_gen[i];
                                } else {
                                    person_msg.name = "unknown:unknown";
                                }
                                person_msg.reliability = 0.0;
                                geometry_msgs::Point p;
                                Point center = Point(dlib2cvrect(current_faces[i]).x + dlib2cvrect(current_faces[i]).width/2.0,
                                                     dlib2cvrect(current_faces[i]).y + dlib2cvrect(current_faces[i]).height/2.0);
                                double mid_x = center.x;
                                double mid_y = center.y;
                                p.x = center.x;
                                p.y = center.y;
                                p.z = dlib2cvrect(current_faces[i]).size().area();
                                person_msg.position = p;
                                people_msg.people.push_back(person_msg);
                             }
                             people_pub.publish(people_msg);
                        }
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

            if (time_spend >= 1 ) {
                average_frames = frame_count;
                time(&start);
                frame_count = 0;
            }

            time(&end);
            time_spend = difftime(end, start);
            string fps = to_string((int)average_frames);
            putText(display_image, "FPS: "+fps, Point2d(display_image.cols-160, 20), fontFace, fontScale, Scalar(255, 255, 255), 1);
            cv::imshow(":: CLF PERCEPTION GENDER AGE ::", display_image);
        }
    }

    ros::shutdown();

    cout << ">>> Cleaning Up. Goodbye!" << endl;

    return 0;
}

