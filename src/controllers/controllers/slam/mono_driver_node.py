#!/usr/bin/env python3


"""
Python node for configuring and driving the MonocularMode cpp node.
Proprocessing may ne performed in this step.
The node may dispatch the received data for logging.

Author: Azmyin Md. Kamal
Date: 01/01/2024

Requirements
* Dataset must be configured in EuRoC MAV format
* Paths to dataset must be set before bulding (or running) this node
* Make sure to set path to your workspace in common.hpp

Command line arguments
-- settings_name: EuRoC, TUM2, KITTI etc; the name of the .yaml file containing camera intrinsics and other configurations
-- image_seq: MH01, V102, etc; the name of the image sequence you want to run

"""

# Imports
#* Import Python modules
import sys # System specific modules
import os # Operating specific functions
import glob
import time # Python timing module
import copy # For deepcopying arrays
import shutil # High level folder operation tool
from pathlib import Path # To find the "home" directory location
import argparse # To accept user arguments from commandline
# import natsort # To ensure all images are chosen loaded in the correct order
import yaml # To manipulate YAML files for reading configuration files
import copy # For making deepcopies of openCV matrices, python lists, numpy arrays etc.
import numpy as np # Python Linear Algebra module
import cv2 # OpenCV

#* ROS2 imports
import ament_index_python.packages
import rclpy
from rclpy.node import Node
from rclpy.parameter import Parameter

# If you have more files in the submodules folder
# from .submodules.py_utils import fn1 # Import helper functions from files in your submodules folder

# Import a custom message interface
# from your_custom_msg_interface.msg import CustomMsg #* Note the camel caps convention

# Import ROS2 message templates
from sensor_msgs.msg import Image # http://wiki.ros.org/sensor_msgs
from std_msgs.msg import String, Float64 # ROS2 string message template
from cv_bridge import CvBridge, CvBridgeError # Library to convert image messages to numpy array
from diagnostic_msgs.msg import DiagnosticStatus
from custom_interfaces.srv import StartupSlam

#* Class definition
class MonoDriver(Node):
    def __init__(self, node_name = "mono_py_node"):
        super().__init__(node_name) # Initializes the rclpy.Node class. It expects the name of the node

        self.declare_parameter("settings_path","/ws/ros_ws/src/slam/config/orbslam3_mono_config.yaml")
        self.declare_parameter("camera_topic", "/camera")
        self.declare_parameter("vocab_file_path", "/ws/config/slam/orbslam3/vocab.txt")
        self.declare_parameter("slam_compute_node_name", "orbslam3_mono_node")
        # Initialize parameters to be passed from the command line (or launch file)
        self.settings_path = str(self.get_parameter('settings_path').value) 
        self.camera_topic = str(self.get_parameter('camera_topic').value)
        self.vocab_file_path = str(self.get_parameter('vocab_file_path').value)
        self.slam_compute_node_name = str(self.get_parameter('slam_compute_node_name').value)

        self.slam_startup_client = self.create_client(StartupSlam, "/{}/start_slam".format(self.slam_compute_node_name))
        while not self.slam_startup_client.wait_for_service(timeout_sec = 1.0):
            self.get_logger().info('service not available, waiting again.....')
        self.startup_req = StartupSlam.Request()
        response = self.send_slam_startup_request(self.settings_path, self.camera_topic)
        print(response.success)
        #* Parse values sent by command line

        # DEBUG
        print(f"-------------- Received parameters --------------------------\n")
        print(f"settings_path: {self.settings_path}")
        print(f"camera_topic: {self.camera_topic}")
        print()

        # Global variables
        self.node_name = "mono_py_driver"

        # Define a CvBridge object for processing images
        self.br = CvBridge()


        # Initialize work variables for main logic
        self.start_frame = 0 # Default 0
        self.end_frame = -1 # Default -1
        self.frame_stop = -1 # Set -1 to use the whole sequence, some positive integer to force sequence to stop, 350 test2, 736 test3
        self.show_imgs = False # Default, False, set True to see the output directly from this node
        self.frame_id = 0 # Integer id of an image frame
        self.frame_count = 0 # Ensure we are consistent with the count number of the frame
        self.inference_time = [] # List to compute average time

        print()
        print(f"MonoDriver initialized, attempting handshake with CPP node")
    # ****************************************************************************************

    # ****************************************************************************************
    def send_slam_startup_request(self, settings_path, camera_topic):
        self.startup_req.config_file_path = settings_path
        self.startup_req.camera_topic = camera_topic
        self.startup_req.camera_type = 'monocular'
        self.slam_startup_future = self.slam_startup_client.call_async(self.startup_req)
        rclpy.spin_until_future_complete(self, self.slam_startup_future)
        return self.slam_startup_future.result()
