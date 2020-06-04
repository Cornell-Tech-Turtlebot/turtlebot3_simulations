/*******************************************************************************
* Copyright 2016 ROBOTIS CO., LTD.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

/* Authors: Taehun Lim (Darby) */

#include "turtlebot3_gazebo/turtlebot3_drive.h"

Turtlebot3Drive::Turtlebot3Drive()
  : nh_priv_("~")
{
  //Init gazebo ros turtlebot3 node
  ROS_INFO("TurtleBot3 Simulation Node Init");
  ROS_ASSERT(init());
}

Turtlebot3Drive::~Turtlebot3Drive()
{
  updatecommandVelocity(0.0, 0.0);
  ros::shutdown();
}

/*******************************************************************************
* Init function
*******************************************************************************/
bool Turtlebot3Drive::init()
{
  // initialize ROS parameter
  std::string cmd_vel_topic_name = nh_.param<std::string>("cmd_vel_topic_name", "");

  // initialize variables
  //escape_range_       = 30.0 * DEG2RAD;
  check_forward_dist_ = 1.0;
  check_side_dist_    = 1.0;

  tb3_pose_ = 0.0;
  prev_tb3_pose_ = 0.0;

  // initialize publishers
  cmd_vel_pub_   = nh_.advertise<geometry_msgs::Twist>(cmd_vel_topic_name, 10);

  // initialize subscribers
  laser_scan_sub_  = nh_.subscribe("scan", 10, &Turtlebot3Drive::laserScanMsgCallBack, this);
  odom_sub_ = nh_.subscribe("odom", 10, &Turtlebot3Drive::odomMsgCallBack, this);

  return true;
}

void Turtlebot3Drive::odomMsgCallBack(const nav_msgs::Odometry::ConstPtr &msg)
{
  double siny = 2.0 * (msg->pose.pose.orientation.w * msg->pose.pose.orientation.z + msg->pose.pose.orientation.x * msg->pose.pose.orientation.y);
	double cosy = 1.0 - 2.0 * (msg->pose.pose.orientation.y * msg->pose.pose.orientation.y + msg->pose.pose.orientation.z * msg->pose.pose.orientation.z);  

	tb3_pose_ = atan2(siny, cosy);
}

bool robot_crashed = false;

void Turtlebot3Drive::laserScanMsgCallBack(const sensor_msgs::LaserScan::ConstPtr &msg)
{
  uint16_t scan_angle[3] = {0, 30, 330};
  int count = 0;

  for (int num = 0; num < 3; num++)
  {
  	if (std::isnan(msg->ranges[num])) {
            count++;
    }

    if (std::isinf(msg->ranges.at(scan_angle[num])))
    {
      scan_data_[num] = msg->range_max;
    }
    else
    {
      scan_data_[num] = msg->ranges.at(scan_angle[num]);
    }
  }

    // Check if the robot has crashed into a wall

    if ((3 * 0.9) < count || scan_data_[1] < 0.25) {
        robot_crashed = true;
    }
    else {
        robot_crashed = false;
    }

}

void Turtlebot3Drive::updatecommandVelocity(double linear, double angular)
{
  geometry_msgs::Twist cmd_vel;

  cmd_vel.linear.x  = linear;
  cmd_vel.angular.z = angular;

  cmd_vel_pub_.publish(cmd_vel);
}

/*******************************************************************************
* Control Loop function - Implements the Left Wall Following ALgorithm
*******************************************************************************/

bool Turtlebot3Drive::controlLoop()
{
	static uint8_t turtlebot3_state_num = 0;

	    
	  switch(turtlebot3_state_num)
	  {

	  	// general case
	    case GET_TB3_DIRECTION:

	    	if(!robot_crashed){ // if the robot hasn't crashed evaluate the situation

		        if (scan_data_[LEFT] > check_side_dist_) // if no left wall move left
		        {
		          prev_tb3_pose_ = tb3_pose_;
		          turtlebot3_state_num = TB3_LEFT_TURN;
		        }
		        else if ((scan_data_[CENTER] > check_forward_dist_)) // else if no front wall move forward
		      	{	    
		      		prev_tb3_pose_ = tb3_pose_;
		      		turtlebot3_state_num = TB3_DRIVE_FORWARD;
		      	}
				else if ((scan_data_[RIGHT] > check_side_dist_)) // else if no right wall move right
				{	 
					prev_tb3_pose_ = tb3_pose_;
		        	turtlebot3_state_num = TB3_RIGHT_TURN;	
				}

		        else // in any other situation rotate right to find a new direction 
		        {
		            prev_tb3_pose_ = tb3_pose_;
		        	turtlebot3_state_num = TB3_TURN_AROUND;	
		        }
		    } else // if the robot has crashed rotate right to find a new direction
		    {
		    	prev_tb3_pose_ = tb3_pose_;
		        turtlebot3_state_num = TB3_TURN_AROUND;	

		    }

		    break; 
	     
	    // moving forward, right, left, or turning around for the case where the robot crashed
	        
	    case TB3_DRIVE_FORWARD:

	    	updatecommandVelocity(LINEAR_VELOCITY, 0.0);
	     	turtlebot3_state_num = GET_TB3_DIRECTION;
	     	break;

	    case TB3_RIGHT_TURN:
		      if (fabs(prev_tb3_pose_ - tb3_pose_) >= escape_range_)
		        turtlebot3_state_num = GET_TB3_DIRECTION;
		      else
		      	updatecommandVelocity(LINEAR_VELOCITY, -1* ANGULAR_VELOCITY);
	      break;

	    case TB3_LEFT_TURN:
	      if (fabs(prev_tb3_pose_ - tb3_pose_) >= escape_range_)
	        turtlebot3_state_num = GET_TB3_DIRECTION;
	      else
	        updatecommandVelocity(LINEAR_VELOCITY, ANGULAR_VELOCITY);
	      break;

	  	case TB3_TURN_AROUND:
	  		updatecommandVelocity(0.0, -1* ANGULAR_VELOCITY);
	      	turtlebot3_state_num = GET_TB3_DIRECTION;
	      break;

	    default:
	      turtlebot3_state_num = GET_TB3_DIRECTION;
	      break;
	  }

  return true; 
}

/*******************************************************************************
* Main function
*******************************************************************************/
int main(int argc, char* argv[])
{
  ros::init(argc, argv, "turtlebot3_drive");
  Turtlebot3Drive turtlebot3_drive;

  ros::Rate loop_rate(125);

  while (ros::ok())
  {
    turtlebot3_drive.controlLoop();
    ros::spinOnce();
    loop_rate.sleep();
  }

  return 0;
}
