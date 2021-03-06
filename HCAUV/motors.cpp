#include "HC.h"

// enable_motor_output() - enable and output lowest possible value to motors
void HC::enable_motor_output()
{
    motors.output_min();
}

// motors_output - send output to motors library which will adjust and send to ESCs and servos
void HC::motors_output()
{
    // Motor detection mode controls the thrusters directly
    int motor_mode = 0;
    if (control_mode == MOTOR_DETECT){
        return;
    }
    
    if(control_mode == ATT_ROBUST){
        motor_mode = 1;
        motors.set_att_force(hc_att_robust_force);
        motors.set_motor_mode(motor_mode);
    }
    else if(control_mode == DEPTH_HOLD_ROBUST){
        motor_mode = 1;
        motors.set_depth_force(hc_depth_hold_robust_force);
        motors.set_motor_mode(motor_mode);
    }
    else if(control_mode == YAW_ROBUST){
        motor_mode = 1;
        hc_yaw_robust_force = constrain_float(hc_yaw_robust_force,-33.26,33.26);
        motors.set_yaw_force(hc_yaw_robust_force);
        motors.set_motor_mode(motor_mode);
    }
    else if(control_mode == YAW_PID){
        motor_mode = 1;
        // motors.set_yaw_robust_force(hc_yaw_robust_force);
        motors.set_motor_mode(motor_mode);
    }
    else if(control_mode == DEPTH_HOLD_PID){
        motor_mode = 1;
        // motors.set_yaw_robust_force(hc_yaw_robust_force);
        motors.set_motor_mode(motor_mode);
    }
    else if(control_mode == ATT_PID){
        motor_mode = 1;
        // motors.set_yaw_robust_force(hc_yaw_robust_force);
        motors.set_motor_mode(motor_mode);
    }
    else if(control_mode == THRUSTER_TEST){
        motor_mode = 2;
        motors.set_motor_mode(motor_mode);
    }
    else{
        motor_mode = 0;
        motors.set_motor_mode(motor_mode);
    }
    // else{
        // check if we are performing the motor test
    if (ap.motor_test) {
        verify_motor_test();
    } else {
        // motor_mode = 1;
        // motors.set_motor_mode(motor_mode);
        // hal.uartC->printf("motors_output\n");
        motors.set_interlock(true);
        motors.output();

    }
    // }
}

// Initialize new style motor test
// Perform checks to see if it is ok to begin the motor test
// Returns true if motor test has begun
bool HC::init_motor_test()
{
    uint32_t tnow = AP_HAL::millis();

    // Ten second cooldown period required with no do_set_motor requests required
    // after failure.
    if (tnow < last_do_motor_test_fail_ms + 10000 && last_do_motor_test_fail_ms > 0) {
        gcs().send_text(MAV_SEVERITY_CRITICAL, "10 second cooldown required after motor test");
        return false;
    }

    // check if safety switch has been pushed
    if (hal.util->safety_switch_state() == AP_HAL::Util::SAFETY_DISARMED) {
        gcs().send_text(MAV_SEVERITY_CRITICAL,"Disarm hardware safety switch before testing motors.");
        return false;
    }

    // Make sure we are on the ground
    if (!motors.armed()) {
        gcs().send_text(MAV_SEVERITY_WARNING, "Arm motors before testing motors.");
        return false;
    }

    enable_motor_output(); // set all motor outputs to zero
    ap.motor_test = true;

    return true;
}

// Verify new style motor test
// The motor test will fail if the interval between received
// MAV_CMD_DO_SET_MOTOR requests exceeds a timeout period
// Returns true if it is ok to proceed with new style motor test
bool HC::verify_motor_test()
{
    bool pass = true;

    // Require at least 2 Hz incoming do_set_motor requests
    if (AP_HAL::millis() > last_do_motor_test_ms + 500) {
        gcs().send_text(MAV_SEVERITY_INFO, "Motor test timed out!");
        pass = false;
    }

    if (!pass) {
        ap.motor_test = false;
        arming.disarm(); // disarm motors
        last_do_motor_test_fail_ms = AP_HAL::millis();
        return false;
    }

    return true;
}

bool HC::handle_do_motor_test(mavlink_command_long_t command) {
    last_do_motor_test_ms = AP_HAL::millis();

    // If we are not already testing motors, initialize test
    static uint32_t tLastInitializationFailed = 0;
    if(!ap.motor_test) {
        // Do not allow initializations attempt under 2 seconds
        // If one fails, we need to give the user time to fix the issue
        // instead of spamming error messages
        if (AP_HAL::millis() > (tLastInitializationFailed + 2000)) {
            if (!init_motor_test()) {
                gcs().send_text(MAV_SEVERITY_WARNING, "motor test initialization failed!");
                tLastInitializationFailed = AP_HAL::millis();
                return false; // init fail
            }
        } else {
            return false;
        }
    }

    float motor_number = command.param1;
    float throttle_type = command.param2;
    float throttle = command.param3;
    // float timeout_s = command.param4; // not used
    // float motor_count = command.param5; // not used
    float test_type = command.param6;

    if (!is_equal(test_type, (float)MOTOR_TEST_ORDER_BOARD)) {
        gcs().send_text(MAV_SEVERITY_WARNING, "bad test type %0.2f", (double)test_type);
        return false; // test type not supported here
    }

    if (is_equal(throttle_type, (float)MOTOR_TEST_THROTTLE_PILOT)) {
        gcs().send_text(MAV_SEVERITY_WARNING, "bad throttle type %0.2f", (double)throttle_type);

        return false; // throttle type not supported here
    }

    if (is_equal(throttle_type, (float)MOTOR_TEST_THROTTLE_PWM)) {
        return motors.output_test_num(motor_number, throttle); // true if motor output is set
    }

    if (is_equal(throttle_type, (float)MOTOR_TEST_THROTTLE_PERCENT)) {
        throttle = constrain_float(throttle, 0.0f, 100.0f);
        throttle = channel_throttle->get_radio_min() + throttle / 100.0f * (channel_throttle->get_radio_max() - channel_throttle->get_radio_min());
        return motors.output_test_num(motor_number, throttle); // true if motor output is set
    }

    return false;
}


// translate wpnav roll/pitch outputs to lateral/forward
void HC::translate_wpnav_rp(float &lateral_out, float &forward_out)
{
    // get roll and pitch targets in centidegrees
    int32_t lateral = wp_nav.get_roll();
    int32_t forward = -wp_nav.get_pitch(); // output is reversed

    // constrain target forward/lateral values
    // The outputs of wp_nav.get_roll and get_pitch should already be constrained to these values
    lateral = constrain_int16(lateral, -aparm.angle_max, aparm.angle_max);
    forward = constrain_int16(forward, -aparm.angle_max, aparm.angle_max);

    // Normalize
    lateral_out = (float)lateral/(float)aparm.angle_max;
    forward_out = (float)forward/(float)aparm.angle_max;
}

// translate wpnav roll/pitch outputs to lateral/forward
void HC::translate_circle_nav_rp(float &lateral_out, float &forward_out)
{
    // get roll and pitch targets in centidegrees
    int32_t lateral = circle_nav.get_roll();
    int32_t forward = -circle_nav.get_pitch(); // output is reversed

    // constrain target forward/lateral values
    lateral = constrain_int16(lateral, -aparm.angle_max, aparm.angle_max);
    forward = constrain_int16(forward, -aparm.angle_max, aparm.angle_max);

    // Normalize
    lateral_out = (float)lateral/(float)aparm.angle_max;
    forward_out = (float)forward/(float)aparm.angle_max;
}

// translate pos_control roll/pitch outputs to lateral/forward
void HC::translate_pos_control_rp(float &lateral_out, float &forward_out)
{
    // get roll and pitch targets in centidegrees
    int32_t lateral = pos_control.get_roll();
    int32_t forward = -pos_control.get_pitch(); // output is reversed

    // constrain target forward/lateral values
    lateral = constrain_int16(lateral, -aparm.angle_max, aparm.angle_max);
    forward = constrain_int16(forward, -aparm.angle_max, aparm.angle_max);

    // Normalize
    lateral_out = (float)lateral/(float)aparm.angle_max;
    forward_out = (float)forward/(float)aparm.angle_max;
}
