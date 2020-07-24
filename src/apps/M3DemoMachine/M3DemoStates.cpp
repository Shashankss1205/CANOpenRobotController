#include "M3DemoStates.h"

double timeval_to_sec(struct timespec *ts)
{
    return (double)(ts->tv_sec + ts->tv_nsec / 1000000000.0);
}




Eigen::Vector3d impedance(Eigen::Matrix3d K, Eigen::Matrix3d D, Eigen::Vector3d X0, Eigen::Vector3d X, Eigen::Vector3d dX, Eigen::Vector3d dXd=Eigen::Vector3d::Zero()) {
    return K*(X0-X) + D*(dXd-dX);
}

//minJerk(X0, Xf, T, t, &X, &dX)
double JerkIt(Eigen::Vector3d X0, Eigen::Vector3d Xf, double T, double t, Eigen::Vector3d &Xd, Eigen::Vector3d &dXd) {
    t = std::max(std::min(t, T), .0); //Bound time
    double tn=std::max(std::min(t/T, 1.0), .0);//Normalised time bounded 0-1
    double tn3=pow(tn,3.);
    double tn4=tn*tn3;
    double tn5=tn*tn4;
    Xd = X0 + ( (X0-Xf) * (15.*tn4-6.*tn5-10.*tn3) );
    dXd = (X0-Xf) * (4.*15.*tn4-5.*6.*tn5-10.*3*tn3)/t;
    return tn;
}


void M3DemoState::entryCode(void) {
    //robot->applyCalibration();
    //robot->initPositionControl();
    robot->initVelocityControl();
    //robot->initTorqueControl();
    qi=robot->getJointPosition();
    Xi=robot->getEndEffPosition();
    robot->setJointVelocity(Eigen::Vector3d::Zero());
}
void M3DemoState::duringCode(void) {
    if(iterations%100==1) {
        //std::cout << "Doing nothing for "<< elapsedTime << "s..." << std::endl;
        //robot->printJointStatus();
        robot->printStatus();
    }
    /*Eigen::Vector3d q = robot->getJointPos();
    q(1)=68*M_PI/180.-0.1*elapsedTime;*/
    //std::cout << q.transpose() <<std::endl;
    //robot->setJointPos(qi-Eigen::Vector3d(0.03,0.03,0.03));
    //double v=-sin(2*M_PI*1./10*elapsedTime);
    //double v=-0.1;
    //robot->setJointVel(Eigen::Vector3d(0,0,0));

    //robot->printStatus();

    /*Eigen::Vector3d dX(-0.02,0.05,0.1);
    if(robot->getEndEffPosition()(2)<0) {
        robot->setEndEffVel(dX);
    }
    else {
        robot->setEndEffVel(Eigen::Vector3d(0,0,0));
    }*/


    /*Eigen::Vector3d Dq;
    if(elapsedTime<5)
        Dq={0,0.015*elapsedTime,0.015*elapsedTime};
    else
        Dq={0,0.015*5.,0.015*5.};
    robot->setJointPos(qi-Dq);*/

    /*Eigen::Vector3d tau(0,-5.0,0);*/
    //robot->setJointTor(robot->calculateGravityTorques());

    /*float b=1.;
    Eigen::Vector3d F(0,0,robot->getEndEffVelocity()[2]);
    robot->setEndEffForceWithCompensation(b*F);*/

    /*float k_i=1.;
    Eigen::Vector3d Xf(-0.4, 0, 0);
    Eigen::Vector3d Xd, dXd;
    JerkIt(Xi, Xf, 5., elapsedTime, Xd, dXd);
    robot->setEndEffVelocity(dXd+k_i*(Xd-robot->getEndEffPosition()));
    std::cout << (Xd-robot->getEndEffPosition()).norm() << std::endl;*/
}
void M3DemoState::exitCode(void) {
    robot->setJointVelocity(Eigen::Vector3d::Zero());
    robot->setEndEffForceWithCompensation(Eigen::Vector3d(0,0,0));
}





void M3CalibState::entryCode(void) {
    calibDone=false;
    for(unsigned int i=0; i<3; i++) {
        stop_reached_time[i] = .0;
        at_stop[i] = false;
    }
    robot->decalibrate();
    robot->initTorqueControl();
    std::cout << "Calibrating (keep clear)...";
}
//Move slowly on each joint until max force detected
void M3CalibState::duringCode(void) {
    Eigen::Vector3d tau(0, 0, 0);

    //Apply constant torque (with damping) unless stop has been detected for more than 0.5s
    Eigen::Vector3d vel=robot->getJointVelocity();
    double b = 3.;
    for(unsigned int i=0; i<3; i++) {
        tau(i) = std::min(std::max(2 - b * vel(i), .0), 2.);
        if(stop_reached_time(i)>0.5) {
            at_stop[i]=true;
        }
        if(vel(i)<0.01) {
            stop_reached_time(i) += dt;
        }
    }

    //Switch to gravity control when done
    if(robot->isCalibrated()) {
        robot->setEndEffForceWithCompensation(Eigen::Vector3d(0,0,0));
        robot->printJointStatus();
        calibDone=true; //Trigger event
    }
    else {
        //If all joints are calibrated
        if(at_stop[0] && at_stop[1] && at_stop[2]) {
            robot->applyCalibration();
            std::cout << "OK." << std::endl;
        }
        else {
            robot->setJointTorque(tau);
        }
    }
}
void M3CalibState::exitCode(void) {
    robot->setEndEffForceWithCompensation(Eigen::Vector3d(0,0,0));
}




void M3MassCompensation::entryCode(void) {
    robot->initTorqueControl();
    std::cout << "Press S to decrease mass (-100g), W to increase (+100g)." << mass << std::endl;
}
void M3MassCompensation::duringCode(void) {

    //Smooth transition in case a mass is set at startup
    double settling_time = 3.0;
    double t=elapsedTime>settling_time?1.0:elapsedTime/settling_time;

    //Bound mass to +-5kg
    if(mass>5.0) {
        mass = 5;
    }
    if(mass<-5) {
        mass = -5;
    }

    //Apply corresponding force
    robot->setEndEffForceWithCompensation(Eigen::Vector3d(0,0,t*mass*9.8));

    //Mass controllable through keyboard inputs
    if(robot->keyboard->getS()) {
        mass -=0.1;
        std::cout << "Mass: " << mass << std::endl;
    }
    if(robot->keyboard->getW()) {
        mass +=0.1;
        std::cout << "Mass: " << mass << std::endl;
    }
}
void M3MassCompensation::exitCode(void) {
    robot->setEndEffForceWithCompensation(Eigen::Vector3d(0,0,0));
}





void M3EndEffDemo::entryCode(void) {
    robot->initVelocityControl();
}
void M3EndEffDemo::duringCode(void) {
    Eigen::Vector3d dX(0,0,0);

    if(elapsedTime<1.0)
    {
        //Go towards the center
        dX={-0.1,0.1,0.1};
    }
    else {
        //Joystick driven
        for(unsigned int i=0; i<3; i++) {
            dX(i)=robot->joystick->getAxis(i)/2.;
        }
    }

    //Apply
    robot->setEndEffVelocity(dX);

    if(iterations%20==1) {
        robot->printStatus();
    }
}
void M3EndEffDemo::exitCode(void) {
    robot->setEndEffVelocity(Eigen::Vector3d(0,0,0));
}




void M3DemoImpedanceState::entryCode(void) {
    robot->initTorqueControl();
    std::cout << "Press Q to select reference point, S/W to tune K gain and A/D for D gain" << std::endl;
}
void M3DemoImpedanceState::duringCode(void) {

    //Select start point
    if(robot->keyboard->getQ()) {
        Xi=robot->getEndEffPosition();
        init=true;
    }

    //K tuning
    if(robot->keyboard->getS()) {
        k -= 5;
        std::cout << "K=" << k << " D=" << d<< std::endl;
    }
    if(robot->keyboard->getW()) {
        k += 5;
        std::cout << "K=" << k << " D=" << d<< std::endl;
    }
    Eigen::Matrix3d K = k*Eigen::Matrix3d::Identity();

    //D tuning
    if(robot->keyboard->getD()) {
        d -= 1;
        std::cout << "K=" << k << " D=" << d<< std::endl;
    }
    if(robot->keyboard->getA()) {
        d += 1;
        std::cout << "K=" << k << " D=" << d<< std::endl;
    }
    Eigen::Matrix3d D = d*Eigen::Matrix3d::Identity();

    //Apply impedance control
    if(init) {
        std::cout << "K=" << k << " D=" << d << " => F=" << impedance(K, Eigen::Matrix3d::Zero(), Xi, robot->getEndEffPosition(), robot->getEndEffVelocity()).transpose() << " N" <<std::endl;
        robot->setEndEffForceWithCompensation(impedance(K, D, Xi, robot->getEndEffPosition(), robot->getEndEffVelocity()));
    }
    else {
        robot->setEndEffForceWithCompensation(Eigen::Vector3d(0,0,0));
    }
}
void M3DemoImpedanceState::exitCode(void) {
    robot->setEndEffForceWithCompensation(Eigen::Vector3d::Zero());
}




void M3SamplingEstimationState::entryCode(void) {
    robot->initTorqueControl();
    robot->setEndEffForceWithCompensation(Eigen::Vector3d::Zero());
    std::cout << "Move robot around while estimating time" << std::endl;
}
void M3SamplingEstimationState::duringCode(void) {
    //Apply gravity compensation
    robot->setEndEffForceWithCompensation(Eigen::Vector3d::Zero());

    //Save dt
    if(iterations<nb_samples) {
        //Do some math for fun
        Eigen::Matrix3d K = 2.36*Eigen::Matrix3d::Identity();
        Eigen::Matrix3d D = 0.235*Eigen::Matrix3d::Identity();
        impedance(K, Eigen::Matrix3d::Zero(), Eigen::Vector3d(-0.5, 0.23, 0.65), robot->getEndEffPosition(), robot->getEndEffVelocity()).transpose();
        robot->J().inverse()*Eigen::Vector3d::Zero()+2*robot->inverseKinematic(Eigen::Vector3d(-0.5, 0, 0));

        //Get time and actual value read from CAN to get sampling rate
        dts[iterations] = dt;
        dX[iterations] = robot->getEndEffVelocity().norm();
        if(dX[iterations-1]!=dX[iterations]){ //Value has actually been updated
            new_value++;
        }
    }
    else if(iterations==nb_samples) {
        std::cout << "Done." <<std::endl;
        double dt_avg=0;
        double dt_max=0;
        double dt_min=50000;
        for(unsigned int i=0; i<nb_samples; i++) {
            dt_avg+=dts[i]/(double)nb_samples;
            if(dts[i]>dt_max)
                dt_max=dts[i];
            if(i>1 && dts[i]<dt_min)
                dt_min=dts[i];
        }
        std::cout<< std::dec<<std::setprecision(3) << "Loop time (min, avg, max): " << dt_min*1000 << " < " << dt_avg*1000 << " < " << dt_max*1000 << " (ms). Actual CAN sampling: " << nb_samples*dt_avg / (double) new_value*1000 <<  std::endl;
    }
}
void M3SamplingEstimationState::exitCode(void) {
    robot->setEndEffForceWithCompensation(Eigen::Vector3d::Zero());
}




void M3DemoMinJerkPosition::entryCode(void) {
    //Setup velocity control for position over velocity loop
    robot->initVelocityControl();
    robot->setJointVelocity(Eigen::Vector3d::Zero());
    //Initialise to first target point
    TrajPtIdx=0;
    startTime=elapsedTime;
    Xi=robot->getEndEffPosition();
    Xf=TrajPt[TrajPtIdx];
    T=TrajTime[TrajPtIdx];
}
void M3DemoMinJerkPosition::duringCode(void) {
    float k_i=1.; //Integral gain
    Eigen::Vector3d Xd, dXd;
    //Compute current desired interpolated point
    double status=JerkIt(Xi, Xf, T, elapsedTime-startTime, Xd, dXd);
    //Apply position control
    robot->setEndEffVelocity(dXd+k_i*(Xd-robot->getEndEffPosition()));

    //Have we reached a point?
    if(status>=1.) {
        //Go to next point
        TrajPtIdx++;
        if(TrajPtIdx>=TrajNbPts){
            TrajPtIdx=0;
        }
        //From where we are
        Xi=robot->getEndEffPosition();
        //To next point
        Xf=TrajPt[TrajPtIdx];
        T=TrajTime[TrajPtIdx];
        startTime=elapsedTime;
    }

    //Display status regularly
    if(iterations%100==1) {
        robot->printStatus();
        std::cout << "Xf="<< Xf.transpose() << " Position error: " << (Xd-robot->getEndEffPosition()).norm() << "m." << std::endl;
    }
}
void M3DemoMinJerkPosition::exitCode(void) {
    robot->setJointVelocity(Eigen::Vector3d::Zero());
}



