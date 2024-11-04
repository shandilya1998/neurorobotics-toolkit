import launch
import launch_ros

def generate_launch_description():
    logger_default = "INFO"
    logger = launch.substitutions.LaunchConfiguration("log-level", default = logger_default)
    logger_arg = launch.actions.DeclareLaunchArgument(
            "log-level",
            default_value=[logger_default],
            description="Logging level")
    nodes = [
        launch_ros.actions.Node(
            package='sensors',
            executable='video_logger_node',
            output='screen',
            namespace='/',
            arguments=['--ros-args', '--log-level', logger],
            name='video_logger',
            parameters = [{
                'output_file_path': '/ws/data/output.mp4',
                'width' : 640,
                'height' : 480,
                'fps' : 30,
                'camera_topic' : '/camera',
                'image_type' : 'bgr'
            }],
            respawn=True
        ),        
        launch_ros.actions.Node(
            package='sensors',
            executable='video_reader_node',
            output='screen',
            namespace='/',
            name='video_reader',
            respawn=True,
            arguments=['--ros-args', '--log-level', logger],
            parameters = [{
                'camera_topic' : "/camera",
                'width' : 640,
                'height' : 480,
                'fps' : 30,
                'input_file_path' : '/ws/data/test.mp4'
            }]
        )
    ]


    return launch.LaunchDescription(nodes)
