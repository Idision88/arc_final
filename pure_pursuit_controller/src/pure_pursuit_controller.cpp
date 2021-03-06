#include "../include/pure_pursuit_controller/pure_pursuit_controller.hpp"
//Parameters
int PURE_PURSUIT_RATE=10;
float STEER_PER_SECOND=0.34;
float K1_LAD_S;
float K2_LAD_S;
float K1_LAD_V;
float K2_LAD_V;
float MU_HAFT; // = 0.8 ungefähr
float G_EARTH;
float MAX_LATERAL_ACCELERATION;
float MAX_ABSOLUTE_VELOCITY;
float DISTANCE_WHEEL_AXIS;
float FOS_VELOCITY;  //[0,1]
float SLOW_DOWN_DISTANCE=10; 
float SLOW_DOWN_PUFFER;
float V_FREEDOM;
float SHUT_DOWN_TIME;
//std::string FILE_LOCATION_PATH_TXT="/home/moritz/.ros/Paths/Obstacles_Hoengg_teach2.txt";
float DISTANCE_INTERPOLATION;
float CRITICAL_OBSTACLE_DISTANCE;

float OBSTACLE_SLOW_DOWN_DISTANCE;
float OBSTACLE_PUFFER_DISTANCE;

float UPPERBOUND_LAD_S;
float LOWERBOUND_LAD_S;
int QUEUE_LENGTH;
std::string STELLGROESSEN_TOPIC;
std::string TRACKING_ERROR_TOPIC;
std::string NAVIGATION_INFO_TOPIC;
std::string STATE_TOPIC;
std::string OBSTACLE_DISTANCE_TOPIC;
std::string SHUTDOWN_TOPIC;
std::string PATH_NAME_EDITED;

// Constructors and Destructors.
// Default Constructor.
PurePursuit::PurePursuit(){}
// Individual Constructor.
PurePursuit::PurePursuit(ros::NodeHandle* n, std::string PATH_NAME )
{	
	n->getParam("/general/PURE_PURSUIT_RATE",PURE_PURSUIT_RATE);
	n->getParam("/general/STEER_PER_SECOND",STEER_PER_SECOND);
	n->getParam("/control/OBSTACLE_SLOW_DOWN_DISTANCE",OBSTACLE_SLOW_DOWN_DISTANCE);
	n->getParam("/control/OBSTACLE_PUFFER_DISTANCE",OBSTACLE_PUFFER_DISTANCE);
	n->getParam("/control/K1_LAD_S", K1_LAD_S);
	n->getParam("/control/K2_LAD_S", K2_LAD_S);
	n->getParam("/control/UPPERBOUND_LAD_S", UPPERBOUND_LAD_S);
	n->getParam("/control/LOWERBOUND_LAD_S", LOWERBOUND_LAD_S);
	n->getParam("/control/K1_LAD_V", K1_LAD_V);
	n->getParam("/control/K2_LAD_V", K2_LAD_V);
	n->getParam("/erod/MU_HAFT",MU_HAFT);
	n->getParam("/erod/G_EARTH",G_EARTH);
	n->getParam("/erod/MAX_LATERAL_ACCELERATION",MAX_LATERAL_ACCELERATION);
	n->getParam("/safety/MAX_ABSOLUTE_VELOCITY",MAX_ABSOLUTE_VELOCITY);
	n->getParam("/erod/DISTANCE_WHEEL_AXIS",DISTANCE_WHEEL_AXIS);
	n->getParam("/safety/FOS_VELOCITY",FOS_VELOCITY);
	n->getParam("/control/SLOW_DOWN_DISTANCE",SLOW_DOWN_DISTANCE);
	n->getParam("/control/SLOW_DOWN_PUFFER",SLOW_DOWN_PUFFER);
	n->getParam("/control/V_FREEDOM",V_FREEDOM);
	n->getParam("/control/SHUT_DOWN_TIME",SHUT_DOWN_TIME );
	n->getParam("/control/DISTANCE_INTERPOLATION",DISTANCE_INTERPOLATION );
	n->getParam("/safety/CRITICAL_OBSTACLE_DISTANCE",CRITICAL_OBSTACLE_DISTANCE );
	n->getParam("/general/QUEUE_LENGTH",QUEUE_LENGTH );
	n->getParam("/topic/STELLGROESSEN",STELLGROESSEN_TOPIC);
	n->getParam("/topic/TRACKING_ERROR",TRACKING_ERROR_TOPIC);
	n->getParam("/topic/NAVIGATION_INFO",NAVIGATION_INFO_TOPIC);
	n->getParam("/topic/STATE",STATE_TOPIC);
	n->getParam("/topic/OBSTACLE_DISTANCE",OBSTACLE_DISTANCE_TOPIC);
	n->getParam("/topic/SHUTDOWN",SHUTDOWN_TOPIC);
	PATH_NAME_EDITED = PATH_NAME + "_teach.txt";  //"/home/moritz/.ros/Paths/current_path.txt";BSP:rosrun pure_pursuit_controller regler_node /home/moritz/.ros/Paths/test

	// 1. Save the arguments to member variables.
	// Set the nodehandle.
	n_ = n;
	// 2. Initialize some member variables.
	// Read in the text file where the teach path is saved and store it to a member variable of type nav_msgs/Path.
	readPathFromTxt(PATH_NAME_EDITED);
	// Initialize gui_stop;
	gui_stop_=0;
	//Initialize obstacle distance in case it is not the first callback function running
	obstacle_distance_=100;
	u_.steering_angle=0;
	u_.speed=0;
	pure_pursuit_gui_msg_.data.clear();
	for(int i=0;i<10;i++) pure_pursuit_gui_msg_.data.push_back(0);
	// 3. ROS specific setups.
	// Publisher.
	// Publishes the control commands to the interface node (TO VCU).
	pub_stellgroessen_ = n_->advertise<ackermann_msgs::AckermannDrive>(STELLGROESSEN_TOPIC, QUEUE_LENGTH);
	// Publishes the track error. Can be used to test the accuracy of the controller.
	track_error_pub_ = n_->advertise<std_msgs::Float64>(TRACKING_ERROR_TOPIC, QUEUE_LENGTH);
	gui_pub_ = n_->advertise<std_msgs::Float32MultiArray>(NAVIGATION_INFO_TOPIC, QUEUE_LENGTH);
	// Subscriber.
	sub_state_ = n_->subscribe(STATE_TOPIC, QUEUE_LENGTH, &PurePursuit::stateCallback,this);
	distance_to_obstacle_sub_=n_->subscribe(OBSTACLE_DISTANCE_TOPIC, QUEUE_LENGTH ,&PurePursuit::obstacleCallback,this);
	gui_stop_sub_=n_->subscribe(SHUTDOWN_TOPIC, QUEUE_LENGTH ,&PurePursuit::guiStopCallback,this);
	// Construction succesful.
	std::cout << std::endl << "PURE PURSUIT: Consturctor with path lenght: " <<n_poses_path_<< " and slow_down_index: "<<slow_down_index_<<std::endl;

//TEST
/*
state_.current_arrayposition=1;
int now=state_.current_arrayposition;
float lad=1.2;
int i=indexOfDistanceFront(now,lad);
float dist_short=distanceIJ(now, i-1);
float dist_long=distanceIJ(now, i);
geometry_msgs::Point point_front=path_.poses[i].pose.position;
geometry_msgs::Point point_back=path_.poses[i-1].pose.position;
geometry_msgs::Point point_interp;
//Interpolate x
point_interp.x=linearInterpolation(point_back.x,point_front.x,dist_short,dist_long,lad);
//Interpolate y
point_interp.y=linearInterpolation(point_back.y,point_front.y,dist_short,dist_long,lad);
//Interpolate z
point_interp.z=linearInterpolation(point_back.z,point_front.z,dist_short,dist_long,lad);

std::cout<<"indexfront= "<<i<<" distanceshort "<<dist_short<<" distancelong "<<dist_long<<std::endl<<" Point front" <<point_front<<" point back "<<point_back<<" point interp "<<point_interp<<std::endl;
*/
//END TEST
}	
// Default destructor.
PurePursuit::~PurePursuit(){}

// Callback Function/Method which waits for new state from L&M and then calculates the ideal control commands.
void PurePursuit::stateCallback(const arc_msgs::State::ConstPtr& incoming_state)
{	
	// Save the incoming state (from subscriber) to a member variable.
	state_ = *incoming_state;
	v_abs_=incoming_state->pose_diff;
	float x_path = path_.poses[state_.current_arrayposition].pose.position.x;
	float y_path = path_.poses[state_.current_arrayposition].pose.position.y;
	float z_path = path_.poses[state_.current_arrayposition].pose.position.z;
	float x_now = state_.pose.pose.position.x;
	float y_now = state_.pose.pose.position.y;
	float z_now = state_.pose.pose.position.z;
	tracking_error_ =abs(arc_tools::globalToLocal(path_.poses[state_.current_arrayposition].pose.position, state_).y);
//	tracking_error_ = sqrt(pow((x_now - x_path),2) + pow((y_now - y_path),2) + pow((z_now - z_path),2));
	pure_pursuit_gui_msg_.data[0]=distanceIJ(0,state_.current_arrayposition);
	pure_pursuit_gui_msg_.data[1]=distanceIJ(state_.current_arrayposition,n_poses_path_-1);
	// Calculate the steering angle using the PurePursuit Controller Formula.
	this -> calculateSteer();
	// Calculate the ideal velocity using the self-derived formula.
	this -> calculateVel();
	// Publish the calculated control commands.
	this -> publishU();
	// Display success.
/*	 std::cout<<"0:Abstand zu Start Pfad "<<pure_pursuit_gui_msg_.data[0]<<std::endl;
	 std::cout<<"1:Abstand zu Ende Pfad "<<pure_pursuit_gui_msg_.data[1]<<std::endl;
	 std::cout<<"2:X-local für steuerung "<<pure_pursuit_gui_msg_.data[2]<<std::endl;
	 std::cout<<"3:Y-local für steuerung"<<pure_pursuit_gui_msg_.data[3]<<std::endl;
	 std::cout<<"4:Lenkwinkel "<<pure_pursuit_gui_msg_.data[4]<<std::endl;
	 std::cout<<"5:Physikalische grenze v_ref "<<pure_pursuit_gui_msg_.data[5]<<std::endl;
	 std::cout<<"6:Teach_geschw+V_Freedom"<<pure_pursuit_gui_msg_.data[6]<<std::endl;
	 std::cout<<"7:Referenzgeschw final "<<pure_pursuit_gui_msg_.data[7]<<std::endl;
	 std::cout<<std::endl;
*/
}

void PurePursuit::guiStopCallback(const std_msgs::Bool::ConstPtr& msg)
{
	gui_stop_=msg->data;
	BigBen_.start();
}

void PurePursuit::obstacleCallback(const std_msgs::Float64::ConstPtr& msg)
{
	obstacle_distance_=msg->data;
}


// Calculate the desired steering angle using the pure pursuit formula.
void PurePursuit::calculateSteer()
{
	// The deviation angle between direction of chassis to direction rearaxle<-->LAD_onPath.
	float theta1;
	// The current speed.
	// Empirical linear function to determine the look-ahead-distance.
	float lad = K2_LAD_S + K1_LAD_S*v_abs_;
	lad=std::max(lad,LOWERBOUND_LAD_S);
	lad=std::min(lad,UPPERBOUND_LAD_S);
	//Eliminate tracking error
	int i=indexOfDistanceFront(state_.current_arrayposition,lad);
	//Approaching end of path set steer to 0
	if(i>=n_poses_path_-1)
	{
		u_.steering_angle=0;
		pure_pursuit_gui_msg_.data[2]=0;
		pure_pursuit_gui_msg_.data[3]=0;
//		pure_pursuit_gui_msg_.data[3]=u_.steering_angle;
	}
	else
	{
//		pure_pursuit_gui_msg_.data[2]=i;
		//Interpolate 
		float dist_short=distanceIJ(state_.current_arrayposition, i-1);
		float dist_long=distanceIJ(state_.current_arrayposition, i);
		geometry_msgs::Point point_front=path_.poses[i].pose.position;
		geometry_msgs::Point point_back=path_.poses[i-1].pose.position;
		geometry_msgs::Point point_interp;
		//Interpolate x
		point_interp.x=linearInterpolation(point_back.x,point_front.x,dist_short,dist_long,lad);
		//Interpolate y
		point_interp.y=linearInterpolation(point_back.y,point_front.y,dist_short,dist_long,lad);
		//Interpolate z
		point_interp.z=linearInterpolation(point_back.z,point_front.z,dist_short,dist_long,lad);
	
		//Project on my plane
		geometry_msgs::Point referenz_local=arc_tools::globalToLocal(point_interp, state_);
		float dy = referenz_local.y;
		float dx = referenz_local.x;
		pure_pursuit_gui_msg_.data[2]=dx;
		pure_pursuit_gui_msg_.data[3]=dy;
//std::cout<<"X-LOCAL "<<dx<<std::endl<<"Y-LOCAL "<<dy<<std::endl;

//std::cout<<"lad "<<lad<<std::endl<<"state \n"<<state_.pose.pose.position<<std::endl<<"ref point \n"<<point_interp<<std::endl<<"local \n"<<referenz_local<<std::endl;
		//PurePursuit formula	
		float alpha = atan2(dy,dx);
		float new_steer= 0.8*atan2(2*DISTANCE_WHEEL_AXIS*sin(alpha),lad);
	
		//Save steer.
		float old_steer = u_.steering_angle;
		u_.steering_angle=new_steer;
		pure_pursuit_gui_msg_.data[4]=u_.steering_angle;
	}
}
// Method which calculates the ideal speed, using the self-derived empirical formula.
void PurePursuit::calculateVel()
{
    //First calculate optimal velocity
	//for the moment take curvature at fix distance lad_v
	float lad_v= K2_LAD_V + K1_LAD_V*v_abs_;
	//find reference index for curvature
	int i=indexOfRadiusFront(state_.current_arrayposition, lad_v);
	if(i>=n_poses_path_) i=n_poses_path_-1;
	float v_limit=20; //sqrt(MAX_LATERAL_ACCELERATION*curveRadius(i));		//Physik stimmt?
	pure_pursuit_gui_msg_.data[5]=v_limit;
    //Upper buonds
	float v_bounded=v_limit;
	//MAX_ABSOLUTE_VELOCITY
	v_bounded=std::min(v_bounded,MAX_ABSOLUTE_VELOCITY);
	//TEACH_VELOCITY
	pure_pursuit_gui_msg_.data[6]=teach_vel_[state_.current_arrayposition-1]+V_FREEDOM;
	v_bounded=std::min(v_bounded,teach_vel_[state_.current_arrayposition-1]+V_FREEDOM);
    //Penalisations
	//Static penalisation
	float C=FOS_VELOCITY;
	//penalise lateral error, half velocity for 2m error
//	C=C/(1+(fabs(tracking_error_)/2));
	//penalise orientation difference to teach part, half at 45deg 
	geometry_msgs::Quaternion quat_teach=path_.poses[state_.current_arrayposition].pose.orientation;
	Eigen::Vector3d euler_teach=arc_tools::YPRFromQuaternion(quat_teach);
	Eigen::Vector3d euler_repeat=arc_tools::YPRFromQuaternion(state_.pose.pose.orientation);
	float delta_yaw=fabs(euler_teach(0)-euler_repeat(0));
    if(delta_yaw>M_PI)
    	delta_yaw=fabs(delta_yaw-2*M_PI);
    if(delta_yaw<8*M_PI/180.0||delta_yaw>-8*M_PI/180.0)
    	delta_yaw=0;
	std::cout << "Derivation DELTA " << delta_yaw/(M_PI)*180 << std::endl;
	float delta_norm=delta_yaw/(M_PI/2);
	C=C/(1+delta_norm*2);
/*	//Obstacle distance
	
	// float brake_dist=pow(v_abs_*3.6/10,2)/2;	//Physikalisch Sinn??
	float brake_dist=10.0;
	pure_pursuit_gui_msg_.data[7]=brake_dist;
	if((obstacle_distance_>brake_dist)&&(obstacle_distance_<2*brake_dist))
	{	
		//std::cout<<"Case1"<<std::endl;	//Comments important at obstacle detection testing
		C=C*(obstacle_distance_/brake_dist)-1;
	}
	else if (obstacle_distance_>=2*brake_dist)
	{
		//std::cout<<"Case2"<<std::endl;
		C=C;
	}
	else if (obstacle_distance_<=brake_dist)
	{
		//std::cout<<"Case3"<<std::endl;
		C=0;
	}
	if(obstacle_distance_<CRITICAL_OBSTACLE_DISTANCE)
	{
		C=0;
	}*/

    //Slow down

	//slow down gradually when arrive at SLOW_DOWN_DISTANCE from end of of path
	if (state_.current_arrayposition>=slow_down_index_)
		{
		std::cout<<"PURE PURSUIT: Slownig down. Distance to end: "<<distanceIJ(state_.current_arrayposition,n_poses_path_-1)<<std::endl;
		//Lineares herrunterschrauben
		C=C*((distanceIJ(state_.current_arrayposition,n_poses_path_-1))-SLOW_DOWN_PUFFER)/(SLOW_DOWN_DISTANCE-SLOW_DOWN_PUFFER);
		}

	//If shut down action is running
	if(gui_stop_==1&&BigBen_.getTimeFromStart()<=SHUT_DOWN_TIME)//&& time zwischen 0 und pi/2
		{
		std::cout<<"PURE PURSUIT: Shutting down gradually"<<std::endl;
		C=C*cos(BigBen_.getTimeFromStart()*1.57079632679/(SHUT_DOWN_TIME));	//Zähler ist PI/2.
		}
	else if (gui_stop_==1 && BigBen_.getTimeFromStart()>SHUT_DOWN_TIME)
		{
		std::cout<<"PURE PURSUIT: Shutted down"<<std::endl;
		C=0;
		}

	//Slow down beacuse of obstacle
/*	if(obstacle_distance_<OBSTACLE_SLOW_DOWN_DISTANCE) std::cout<<OBSTACLE_SLOW_DOWN_DISTANCE<<" PURE PURSUIT; Slow down for obstacle"<<std::endl;
	if(obstacle_distance_<OBSTACLE_SLOW_DOWN_DISTANCE && obstacle_distance_>=OBSTACLE_PUFFER_DISTANCE) {
		C=0.4;
	}
	if(obstacle_distance_<OBSTACLE_PUFFER_DISTANCE) {
		C=0.0;
	}
	*/	
	obstacle_distance_=std::max(obstacle_distance_,OBSTACLE_PUFFER_DISTANCE);
	obstacle_distance_=std::min(obstacle_distance_,OBSTACLE_SLOW_DOWN_DISTANCE);
	C=C * (obstacle_distance_ - OBSTACLE_PUFFER_DISTANCE) / (OBSTACLE_SLOW_DOWN_DISTANCE - OBSTACLE_PUFFER_DISTANCE);

	float v_ref = v_bounded * C;
    //Speichern auf Stellgrössen
	u_.speed=6;//v_ref;
	pure_pursuit_gui_msg_.data[7]=u_.speed;
	u_.acceleration=v_abs_;
}
// Method which publishes the calculated commands onto the topic to the system engineers interface node.
void PurePursuit::publishU()
{
	pub_stellgroessen_.publish(u_);
	gui_pub_.publish(pure_pursuit_gui_msg_);
}

// Method which reads in the text file and saves the path to the path_variable.
void PurePursuit::readPathFromTxt(std::string inFileName)
{
	// Create an ifstream object.
	std::fstream fin;
	fin.open(inFileName.c_str());
	
	// Check if stream is open.
	if (!fin.is_open())
	{
		std::cout << "PURE PURSUIT: Error with opening of  " <<inFileName << std::endl;
	}

	// Truncate two lines of the file to get rid of the last '|'.
	fin.seekg (-2, fin.end);
	int length = fin.tellg();
	fin.seekg (0, fin.beg);
	//Stream erstellen mit chars von fin.
	char *file = new char [length];
	fin.read(file, length);
	std::istringstream stream(file, std::ios::in);
	delete[] file;
	fin.close();

	int i = 0;
	int j;
	geometry_msgs::PoseStamped temp_pose;

	// Save to path_ variable.
	while(!stream.eof() && i<length)
	{
		geometry_msgs::PoseStamped temp_pose;
		float temp_diff;
		path_.poses.push_back(temp_pose);
		teach_vel_.push_back(temp_diff);
		path_diff_.poses.push_back(temp_pose);
		stream>>j;
		stream>>path_.poses[j-1].pose.position.x;
		stream>>path_.poses[j-1].pose.position.y;
		stream>>path_.poses[j-1].pose.position.z;
		//Save orientation
		stream>>path_.poses[j-1].pose.orientation.x;
		stream>>path_.poses[j-1].pose.orientation.y;
		stream>>path_.poses[j-1].pose.orientation.z;
		stream>>path_.poses[j-1].pose.orientation.w;
		//Save teach_velocity
		stream>>teach_vel_[j-1];//path_diff_.poses[j-1].pose.position.x;
						//stream>>path_diff_.poses[j-1].pose.position.y;
						//stream>>path_diff_.poses[j-1].pose.position.z;

		stream.ignore (300, '|');
		i++;
	}
	n_poses_path_ = i;
	float l_dumb=0;
	i=n_poses_path_-1;
	slow_down_index_=indexOfDistanceBack(i,SLOW_DOWN_DISTANCE);
}


float PurePursuit::distanceIJ(int from_i , int to_i )	//Achtung indices: es muss auch für to_i=from_i+1 gehen, sonst unendliche schleife bei readpathfrom txt;
{	
	float d=0;
	for (int i =from_i; i<to_i; i++)
	{
		d += sqrt(	pow(path_.poses[i].pose.position.x - path_.poses[i+1].pose.position.x,2)+
				pow(path_.poses[i].pose.position.y - path_.poses[i+1].pose.position.y,2)+
				pow(path_.poses[i].pose.position.z - path_.poses[i+1].pose.position.z,2));
		if((i+1)>n_poses_path_-1){std::cout<<"PURE PURSUIT: LAUFZEITFEHLER distanceIJ"<<std::endl;}
	}
	return d;

}

float PurePursuit::curveRadius(int j)
{	
	int count=0;
	float r_sum=0;
	for(int t=1;t<=3;t++)
	{	
		count++;
		float D=DISTANCE_INTERPOLATION/t;
		int i=j;
		int n_front=indexOfDistanceFront(i-1,D);
		int n_back=indexOfDistanceBack(i-1,D);
		if(n_back<=0)
		{
			i=indexOfDistanceFront(0,D);
			n_front=indexOfDistanceFront(i-1,D);
			n_back=indexOfDistanceBack(i-1,D);
		}
		else if(n_front>=n_poses_path_-1)
		{
			i=indexOfDistanceBack(n_poses_path_-1,D);
			n_front=indexOfDistanceFront(i-1,D);
			n_back=indexOfDistanceBack(i-1,D);
		}

		Eigen::Vector3d i_back(		path_.poses[n_back].pose.position.x-path_.poses[i].pose.position.x,
							path_.poses[n_back].pose.position.y-path_.poses[i].pose.position.y,
							path_.poses[n_back].pose.position.z-path_.poses[i].pose.position.z);
		Eigen::Vector3d i_front(	path_.poses[n_front].pose.position.x-path_.poses[i].pose.position.x,
							path_.poses[n_front].pose.position.y-path_.poses[i].pose.position.y,
							path_.poses[n_front].pose.position.z-path_.poses[i].pose.position.z);
		Eigen::Vector3d back_front(	path_.poses[n_front].pose.position.x-path_.poses[n_back].pose.position.x,
							path_.poses[n_front].pose.position.y-path_.poses[n_back].pose.position.y,
							path_.poses[n_front].pose.position.z-path_.poses[n_back].pose.position.z);
		if((n_back>n_poses_path_-1)&&(n_front>n_poses_path_-1)&&(i>n_poses_path_-1))
			{std::cout<<"PURE PURSUIT: LAUFZEITFEHLER curve radius"<<std::endl;}
		float zaehler =i_back.dot(-i_front);
		float nenner = (i_back.norm()*i_front.norm());
		float argument=zaehler/nenner;
		if (argument>1) argument=1;
		if (argument<-1) argument=-1;
		float gamma=acos(argument);//winkel zwischen Vektoren
		if(fabs(sin(gamma))<0.001) 
		{
			r_sum+=9999999;		//irgendeine grosse Zahl um nicht nan zu erzeugen in nächster zeile
		}
		else
		{
			r_sum+=back_front.norm()/(2*fabs(sin(gamma)));	//Gleichung umkreis
		}
	}
	float r=r_sum/count;
//	pure_pursuit_gui_msg_.data[5]=r;
	return r;
}

int PurePursuit::indexOfDistanceFront(int i, float d)
{
	int j=i;
	float l = 0;
	while(l<d &&j<n_poses_path_-1)
	{
		l += sqrt(	pow(path_.poses[j+1].pose.position.x - path_.poses[j].pose.position.x,2)+
				pow(path_.poses[j+1].pose.position.y - path_.poses[j].pose.position.y,2)+
				pow(path_.poses[j+1].pose.position.z - path_.poses[j].pose.position.z,2));
		if(j+1>n_poses_path_-1){std::cout<<"PURE PURSUIT: LAUFZEITFEHLER::indexOfDistanceFront"<<std::endl;}
		j ++;
	}
	return j;
}

int PurePursuit::indexOfDistanceBack(int i, float d)
{
	int j=i;
	float l = 0;
	while(l<d && j>0)
	{
		l += sqrt(	pow(path_.poses[j-1].pose.position.x - path_.poses[j].pose.position.x,2)+
				pow(path_.poses[j-1].pose.position.y - path_.poses[j].pose.position.y,2)+
				pow(path_.poses[j-1].pose.position.z - path_.poses[j].pose.position.z,2));
		if(j>n_poses_path_-1){std::cout<<"PURE PURSUIT: LAUFZEITFEHLER indexOfDistanceBack"<<std::endl;}
		j --;
	}
	return j;
}

int PurePursuit::indexOfRadiusFront(int i_start, float d)
{	
	int j=i_start;
	float d_old=100;
	float d_new; 
	geometry_msgs::Point my_point=path_.poses[i_start].pose.position;
	geometry_msgs::Point search_point;
	int i_end=indexOfDistanceFront(i_start, 30);	//30 soll viel grösser sein als möglicher suchradius..
	for(int i=i_start; i<i_end ; i++ )
		{
		search_point=path_.poses[i].pose.position;
		d_new=fabs(d-sqrt(pow((my_point.x-search_point.x),2)+pow((my_point.y-search_point.y),2)));
		if(d_new<d_old)
			{
			d_old=d_new;
			j=i;
			}
		}
	return j;
}

// Method which returns the current state.
arc_msgs::State PurePursuit::getState()
{
	return state_;
}
// Method which returns the calculated control commands.
ackermann_msgs::AckermannDrive PurePursuit::getU()
{
	ackermann_msgs::AckermannDrive u1 = u_;
	return u1;
}

float PurePursuit::linearInterpolation(float a_lower, float a_upper ,float b_lower, float b_upper, float b_middle)
{	
	if(b_upper == b_lower) 
	{
		std::cout<<"Falsch interpoliert \n";
		return a_lower;
	}
	float a_middle =  a_lower + ( b_middle - b_lower ) * ( a_upper - a_lower )/( b_upper - b_lower );
	return a_middle;
}
