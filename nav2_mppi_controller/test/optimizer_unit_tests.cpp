// Copyright (c) 2022 Samsung Research America, @artofnothingness Alexey Budyakov
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <chrono>
#include <thread>

#include "gtest/gtest.h"
#include "rclcpp/rclcpp.hpp"
#include "nav2_mppi_controller/optimizer.hpp"
#include "tf2_ros/buffer.h"

// Tests main optimizer functions

using namespace mppi;  // NOLINT
using namespace mppi::critics;  // NOLINT
using namespace mppi::utils;  // NOLINT

class OptimizerTester : public Optimizer
{
public:
  OptimizerTester()
  : Optimizer() {}

  void testSetDiffModel()
  {
    EXPECT_EQ(motion_model_.get(), nullptr);
    EXPECT_NO_THROW(setMotionModel("DiffDrive"));
    EXPECT_NE(motion_model_.get(), nullptr);
    EXPECT_TRUE(dynamic_cast<DiffDriveMotionModel *>(motion_model_.get()));
    EXPECT_FALSE(isHolonomic());
  }

  void testSetOmniModel()
  {
    EXPECT_EQ(motion_model_.get(), nullptr);
    EXPECT_NO_THROW(setMotionModel("Omni"));
    EXPECT_NE(motion_model_.get(), nullptr);
    EXPECT_TRUE(dynamic_cast<OmniMotionModel *>(motion_model_.get()));
    EXPECT_TRUE(isHolonomic());
  }

  void testSetAckModel()
  {
    EXPECT_EQ(motion_model_.get(), nullptr);
    EXPECT_NO_THROW(setMotionModel("Ackermann"));
    EXPECT_NE(motion_model_.get(), nullptr);
    EXPECT_TRUE(dynamic_cast<AckermannMotionModel *>(motion_model_.get()));
    EXPECT_FALSE(isHolonomic());
  }

  void testSetRandModel()
  {
    EXPECT_EQ(motion_model_.get(), nullptr);
    try {
      setMotionModel("Random");
      FAIL();
    } catch (...) {
      SUCCEED();
    }
    EXPECT_EQ(motion_model_.get(), nullptr);
  }

  void resetMotionModel()
  {
    motion_model_.reset();
  }

  void setOffsetWrapper(const double freq)
  {
    return setOffset(freq);
  }

  bool getShiftControlSequence()
  {
    return settings_.shift_control_sequence;
  }

  void fillOptimizerWithGarbage()
  {
    state_.vx = 0.43432 * Eigen::ArrayXXf::Ones(1000, 10);
    control_sequence_.vx = 342.0 * Eigen::ArrayXf::Ones(30);
    control_history_[0] = {43, 5646, 32432};
    costs_ = 5.32 * Eigen::ArrayXf::Ones(56453);
    generated_trajectories_.x = 432.234 * Eigen::ArrayXf::Ones(7865);
  }

  void testReset()
  {
    reset();

    EXPECT_TRUE(state_.vx.isApproxToConstant(0.0f));
    EXPECT_TRUE(control_sequence_.vx.isApproxToConstant(0.0f));
    EXPECT_EQ(control_history_[0].vx, 0.0);
    EXPECT_EQ(control_history_[0].vy, 0.0);
    EXPECT_NEAR(costs_.sum(), 0, 1e-6);
    EXPECT_TRUE(generated_trajectories_.x.isApproxToConstant(0.0f));
  }

  bool fallbackWrapper(bool fail)
  {
    return fallback(fail);
  }

  void testPrepare(
    const geometry_msgs::msg::PoseStamped & robot_pose,
    const geometry_msgs::msg::Twist & robot_speed,
    const nav_msgs::msg::Path & plan,
    const geometry_msgs::msg::Pose & goal,
    nav2_core::GoalChecker * goal_checker)
  {
    prepare(robot_pose, robot_speed, plan, goal, goal_checker);

    EXPECT_EQ(critics_data_.goal_checker, nullptr);
    EXPECT_NEAR(costs_.sum(), 0, 1e-6);  // should be reset
    EXPECT_FALSE(critics_data_.fail_flag);  // should be reset
    EXPECT_FALSE(critics_data_.motion_model->isHolonomic());  // object is valid + diff drive
    EXPECT_FALSE(critics_data_.furthest_reached_path_point.has_value());  // val is not set
    EXPECT_FALSE(critics_data_.path_pts_valid.has_value());  // val is not set
    EXPECT_EQ(state_.pose.pose.position.x, 999);
    EXPECT_EQ(state_.speed.linear.y, 4.0);
    EXPECT_EQ(path_.x.rows(), 17);
  }

  void shiftControlSequenceWrapper()
  {
    return shiftControlSequence();
  }

  std::pair<double, double> getVelLimits()
  {
    auto & s = settings_;
    return {s.constraints.vx_min, s.constraints.vx_max};
  }

  models::ControlConstraints & getControlConstraints()
  {
    return settings_.constraints;
  }

  void applyControlSequenceConstraintsWrapper()
  {
    return applyControlSequenceConstraints();
  }

  models::ControlSequence & grabControlSequence()
  {
    return control_sequence_;
  }

  void testupdateStateVels()
  {
    // updateInitialStateVelocities
    models::State state;
    state.reset(1000, 50);
    state.speed.linear.x = 5.0;
    state.speed.linear.y = 1.0;
    state.speed.angular.z = 6.0;
    state.cvx = 0.75 * Eigen::ArrayXXf::Ones(1000, 50);
    state.cvy = 0.5 * Eigen::ArrayXXf::Ones(1000, 50);
    state.cwz = 0.1 * Eigen::ArrayXXf::Ones(1000, 50);
    updateInitialStateVelocities(state);
    EXPECT_NEAR(state.vx(0, 0), 5.0, 1e-6);
    EXPECT_NEAR(state.vy(0, 0), 1.0, 1e-6);
    EXPECT_NEAR(state.wz(0, 0), 6.0, 1e-6);

    // propagateStateVelocitiesFromInitials
    float max_delta_vx = settings_.constraints.ax_max * settings_.model_dt;
    float min_delta_vx = settings_.constraints.ax_min * settings_.model_dt;
    float max_delta_vy = settings_.constraints.ay_max * settings_.model_dt;
    float min_delta_vy = settings_.constraints.ay_min * settings_.model_dt;
    float max_delta_wz = settings_.constraints.az_max * settings_.model_dt;
    propagateStateVelocitiesFromInitials(state);
    EXPECT_NEAR(state.vx(0, 0), 5.0, 1e-6);
    EXPECT_NEAR(state.vy(0, 0), 1.0, 1e-6);
    EXPECT_NEAR(state.wz(0, 0), 6.0, 1e-6);
    EXPECT_NEAR(state.vx(0, 1), std::clamp(0.75, 5.0 + min_delta_vx, 5.0 + max_delta_vx), 1e-6);
    EXPECT_NEAR(state.vy(0, 1), std::clamp(0.5, 1.0 + min_delta_vy, 1.0 + max_delta_vy), 1e-6);
    EXPECT_NEAR(state.wz(0, 1), std::clamp(0.1, 6.0 - max_delta_wz, 6.0 + max_delta_wz), 1e-6);

    // Putting them together: updateStateVelocities
    state.reset(1000, 50);
    state.speed.linear.x = -5.0;
    state.speed.linear.y = -1.0;
    state.speed.angular.z = -6.0;
    state.cvx = -0.75 * Eigen::ArrayXXf::Ones(1000, 50);
    state.cvy = -0.5 * Eigen::ArrayXXf::Ones(1000, 50);
    state.cwz = -0.1 * Eigen::ArrayXXf::Ones(1000, 50);
    updateStateVelocities(state);
    EXPECT_NEAR(state.vx(0, 0), -5.0, 1e-6);
    EXPECT_NEAR(state.vy(0, 0), -1.0, 1e-6);
    EXPECT_NEAR(state.wz(0, 0), -6.0, 1e-6);
    EXPECT_NEAR(state.vx(0, 1), std::clamp(-0.75, -5.0 + min_delta_vx, -5.0 + max_delta_vx), 1e-6);
    EXPECT_NEAR(state.vy(0, 1), std::clamp(-0.5, -1.0 - max_delta_vy, -1.0 + max_delta_vy), 1e-6);
    EXPECT_NEAR(state.wz(0, 1), std::clamp(-0.1, -6.0 - max_delta_wz, -6.0 + max_delta_wz), 1e-6);
  }

  geometry_msgs::msg::TwistStamped getControlFromSequenceAsTwistWrapper()
  {
    builtin_interfaces::msg::Time stamp;
    return getControlFromSequenceAsTwist(stamp);
  }

  void integrateStateVelocitiesWrapper(
    models::Trajectories & traj,
    const models::State & state)
  {
    return integrateStateVelocities(traj, state);
  }
};

TEST(OptimizerTests, BasicInitializedFunctions)
{
  auto node = std::make_shared<nav2::LifecycleNode>("my_node");
  OptimizerTester optimizer_tester;
  node->declare_parameter("mppic.batch_size", rclcpp::ParameterValue(1000));
  node->declare_parameter("mppic.time_steps", rclcpp::ParameterValue(50));
  node->declare_parameter("controller_frequency", rclcpp::ParameterValue(30.0));
  node->declare_parameter("mppic.ax_min", rclcpp::ParameterValue(3.0));
  node->declare_parameter("mppic.ay_min", rclcpp::ParameterValue(3.0));
  auto costmap_ros = std::make_shared<nav2_costmap_2d::Costmap2DROS>(
    "dummy_costmap", "", true);
  std::string name = "test";
  ParametersHandler param_handler(node, name);
  rclcpp_lifecycle::State lstate;
  costmap_ros->on_configure(lstate);
  auto tf_buffer = std::make_shared<tf2_ros::Buffer>(node->get_clock());
  optimizer_tester.initialize(node, "mppic", costmap_ros, tf_buffer, &param_handler);

  // Test value of ax_min, ay_min it should be negative
  auto & constraints = optimizer_tester.getControlConstraints();
  EXPECT_EQ(constraints.ax_min, -3.0);
  EXPECT_EQ(constraints.ay_min, -3.0);

  // Should be empty of size batches x time steps
  // and tests getting set params: time_steps, batch_size, controller_frequency
  auto trajs = optimizer_tester.getGeneratedTrajectories();
  EXPECT_EQ(trajs.x.rows(), 1000);
  EXPECT_EQ(trajs.x.cols(), 50);
  EXPECT_TRUE(trajs.x.isApproxToConstant(0.0f));

  optimizer_tester.resetMotionModel();
  optimizer_tester.testSetOmniModel();
  auto traj = optimizer_tester.getOptimizedTrajectory();
  EXPECT_EQ(traj(5, 0), 0.0);  // x
  EXPECT_EQ(traj(5, 1), 0.0);  // y
  EXPECT_EQ(traj(5, 2), 0.0);  // yaw
  EXPECT_EQ(traj.rows(), 50);
  EXPECT_EQ(traj.cols(), 3);

  optimizer_tester.reset();
  optimizer_tester.shutdown();
}

TEST(OptimizerTests, TestOptimizerMotionModels)
{
  auto node = std::make_shared<nav2::LifecycleNode>("my_node");
  OptimizerTester optimizer_tester;
  node->declare_parameter("controller_frequency", rclcpp::ParameterValue(30.0));
  auto costmap_ros = std::make_shared<nav2_costmap_2d::Costmap2DROS>(
    "dummy_costmap", "", true);
  std::string name = "test";
  ParametersHandler param_handler(node, name);
  rclcpp_lifecycle::State lstate;
  costmap_ros->on_configure(lstate);
  auto tf_buffer = std::make_shared<tf2_ros::Buffer>(node->get_clock());
  optimizer_tester.initialize(node, "mppic", costmap_ros, tf_buffer, &param_handler);

  // Diff Drive should be non-holonomic
  optimizer_tester.resetMotionModel();
  optimizer_tester.testSetDiffModel();

  // Omni Drive should be holonomic
  optimizer_tester.resetMotionModel();
  optimizer_tester.testSetOmniModel();

  // // Ackermann should be non-holonomic
  optimizer_tester.resetMotionModel();
  optimizer_tester.testSetAckModel();

  // // Rand should fail
  optimizer_tester.resetMotionModel();
  optimizer_tester.testSetRandModel();
}

TEST(OptimizerTests, setOffsetTests)
{
  auto node = std::make_shared<nav2::LifecycleNode>("my_node");
  OptimizerTester optimizer_tester;
  node->declare_parameter("mppic.model_dt", rclcpp::ParameterValue(0.1));
  node->declare_parameter("controller_frequency", rclcpp::ParameterValue(30.0));
  node->declare_parameter("mppic.batch_size", rclcpp::ParameterValue(1000));
  node->declare_parameter("mppic.time_steps", rclcpp::ParameterValue(50));
  auto costmap_ros = std::make_shared<nav2_costmap_2d::Costmap2DROS>(
    "dummy_costmap", "", true);
  std::string name = "test";
  ParametersHandler param_handler(node, name);
  rclcpp_lifecycle::State lstate;
  costmap_ros->on_configure(lstate);
  auto tf_buffer = std::make_shared<tf2_ros::Buffer>(node->get_clock());
  optimizer_tester.initialize(node, "mppic", costmap_ros, tf_buffer, &param_handler);

  // Test offsets are properly set based on relationship of model_dt and controller frequency
  // Also tests getting set model_dt parameter.
  EXPECT_THROW(optimizer_tester.setOffsetWrapper(1.0), std::runtime_error);
  EXPECT_NO_THROW(optimizer_tester.setOffsetWrapper(30.0));
  EXPECT_FALSE(optimizer_tester.getShiftControlSequence());
  EXPECT_NO_THROW(optimizer_tester.setOffsetWrapper(10.0));
  EXPECT_TRUE(optimizer_tester.getShiftControlSequence());
}

TEST(OptimizerTests, resetTests)
{
  auto node = std::make_shared<nav2::LifecycleNode>("my_node");
  OptimizerTester optimizer_tester;
  node->declare_parameter("controller_frequency", rclcpp::ParameterValue(30.0));
  node->declare_parameter("mppic.batch_size", rclcpp::ParameterValue(1000));
  node->declare_parameter("mppic.time_steps", rclcpp::ParameterValue(50));
  auto costmap_ros = std::make_shared<nav2_costmap_2d::Costmap2DROS>(
    "dummy_costmap", "", true);
  std::string name = "test";
  ParametersHandler param_handler(node, name);
  rclcpp_lifecycle::State lstate;
  costmap_ros->on_configure(lstate);
  auto tf_buffer = std::make_shared<tf2_ros::Buffer>(node->get_clock());
  optimizer_tester.initialize(node, "mppic", costmap_ros, tf_buffer, &param_handler);

  // Tests resetting the full state of all the functions after filling with garbage
  optimizer_tester.fillOptimizerWithGarbage();
  optimizer_tester.testReset();
}

TEST(OptimizerTests, FallbackTests)
{
  auto node = std::make_shared<nav2::LifecycleNode>("my_node");
  OptimizerTester optimizer_tester;
  node->declare_parameter("controller_frequency", rclcpp::ParameterValue(30.0));
  node->declare_parameter("mppic.batch_size", rclcpp::ParameterValue(1000));
  node->declare_parameter("mppic.time_steps", rclcpp::ParameterValue(50));
  node->declare_parameter("mppic.retry_attempt_limit", rclcpp::ParameterValue(2));
  auto costmap_ros = std::make_shared<nav2_costmap_2d::Costmap2DROS>(
    "dummy_costmap", "", true);
  std::string name = "test";
  ParametersHandler param_handler(node, name);
  rclcpp_lifecycle::State lstate;
  costmap_ros->on_configure(lstate);
  auto tf_buffer = std::make_shared<tf2_ros::Buffer>(node->get_clock());
  optimizer_tester.initialize(node, "mppic", costmap_ros, tf_buffer, &param_handler);

  // Test fallback logic, also tests getting set param retry_attempt_limit
  // Because retry set to 2, it should attempt soft resets 2x before throwing exception
  // for hard reset
  EXPECT_FALSE(optimizer_tester.fallbackWrapper(false));
  EXPECT_TRUE(optimizer_tester.fallbackWrapper(true));
  EXPECT_TRUE(optimizer_tester.fallbackWrapper(true));
  EXPECT_THROW(optimizer_tester.fallbackWrapper(true), std::runtime_error);
}

TEST(OptimizerTests, PrepareTests)
{
  auto node = std::make_shared<nav2::LifecycleNode>("my_node");
  OptimizerTester optimizer_tester;
  node->declare_parameter("controller_frequency", rclcpp::ParameterValue(30.0));
  node->declare_parameter("mppic.batch_size", rclcpp::ParameterValue(1000));
  node->declare_parameter("mppic.time_steps", rclcpp::ParameterValue(50));
  node->declare_parameter("mppic.retry_attempt_limit", rclcpp::ParameterValue(2));
  auto costmap_ros = std::make_shared<nav2_costmap_2d::Costmap2DROS>(
    "dummy_costmap", "", true);
  std::string name = "test";
  ParametersHandler param_handler(node, name);
  rclcpp_lifecycle::State lstate;
  costmap_ros->on_configure(lstate);
  auto tf_buffer = std::make_shared<tf2_ros::Buffer>(node->get_clock());
  optimizer_tester.initialize(node, "mppic", costmap_ros, tf_buffer, &param_handler);

  // Test Prepare function to set the state of the robot pose/speed on new cycle
  // Populate the contents with things easily identifiable if correct
  geometry_msgs::msg::PoseStamped pose;
  pose.pose.position.x = 999;
  geometry_msgs::msg::Twist speed;
  speed.linear.y = 4.0;
  nav_msgs::msg::Path path;
  geometry_msgs::msg::Pose goal;
  path.poses.resize(17);

  optimizer_tester.testPrepare(pose, speed, path, goal, nullptr);
}

TEST(OptimizerTests, shiftControlSequenceTests)
{
  auto node = std::make_shared<nav2::LifecycleNode>("my_node");
  OptimizerTester optimizer_tester;
  node->declare_parameter("controller_frequency", rclcpp::ParameterValue(30.0));
  node->declare_parameter("mppic.batch_size", rclcpp::ParameterValue(1000));
  node->declare_parameter("mppic.time_steps", rclcpp::ParameterValue(50));
  node->declare_parameter("mppic.retry_attempt_limit", rclcpp::ParameterValue(2));
  auto costmap_ros = std::make_shared<nav2_costmap_2d::Costmap2DROS>(
    "dummy_costmap", "", true);
  std::string name = "test";
  ParametersHandler param_handler(node, name);
  rclcpp_lifecycle::State lstate;
  costmap_ros->on_configure(lstate);
  auto tf_buffer = std::make_shared<tf2_ros::Buffer>(node->get_clock());
  optimizer_tester.initialize(node, "mppic", costmap_ros, tf_buffer, &param_handler);

  // Test shiftControlSequence by setting the 2nd value to something unique to neighbors
  auto & sequence = optimizer_tester.grabControlSequence();
  sequence.reset(100);
  sequence.vx(0) = 9999;
  sequence.vx(1) = 6;
  sequence.vx(2) = 888;
  sequence.vy(0) = 9999;
  sequence.vy(1) = 6;
  sequence.vy(2) = 888;
  sequence.wz(0) = 9999;
  sequence.wz(1) = 6;
  sequence.wz(2) = 888;

  optimizer_tester.resetMotionModel();
  optimizer_tester.testSetOmniModel();
  optimizer_tester.shiftControlSequenceWrapper();

  EXPECT_EQ(sequence.vx(0), 6);
  EXPECT_EQ(sequence.vy(0), 6);
  EXPECT_EQ(sequence.wz(0), 6);
  EXPECT_EQ(sequence.vx(1), 888);
  EXPECT_EQ(sequence.vy(1), 888);
  EXPECT_EQ(sequence.wz(1), 888);
  EXPECT_EQ(sequence.vx(2), 0);
  EXPECT_EQ(sequence.vy(2), 0);
  EXPECT_EQ(sequence.wz(2), 0);
}

TEST(OptimizerTests, SpeedLimitTests)
{
  auto node = std::make_shared<nav2::LifecycleNode>("my_node");
  OptimizerTester optimizer_tester;
  node->declare_parameter("controller_frequency", rclcpp::ParameterValue(30.0));
  node->declare_parameter("mppic.batch_size", rclcpp::ParameterValue(1000));
  node->declare_parameter("mppic.time_steps", rclcpp::ParameterValue(50));
  node->declare_parameter("mppic.retry_attempt_limit", rclcpp::ParameterValue(2));
  auto costmap_ros = std::make_shared<nav2_costmap_2d::Costmap2DROS>(
    "dummy_costmap", "", true);
  std::string name = "test";
  ParametersHandler param_handler(node, name);
  rclcpp_lifecycle::State lstate;
  costmap_ros->on_configure(lstate);
  auto tf_buffer = std::make_shared<tf2_ros::Buffer>(node->get_clock());
  optimizer_tester.initialize(node, "mppic", costmap_ros, tf_buffer, &param_handler);

  // Test Speed limits API
  auto [v_min, v_max] = optimizer_tester.getVelLimits();
  EXPECT_EQ(v_max, 0.5f);
  EXPECT_EQ(v_min, -0.35f);
  optimizer_tester.setSpeedLimit(0, false);
  auto [v_min2, v_max2] = optimizer_tester.getVelLimits();
  EXPECT_EQ(v_max2, 0.5f);
  EXPECT_EQ(v_min2, -0.35f);
  optimizer_tester.setSpeedLimit(50.0, true);
  auto [v_min3, v_max3] = optimizer_tester.getVelLimits();
  EXPECT_NEAR(v_max3, 0.5 / 2.0, 1e-3);
  EXPECT_NEAR(v_min3, -0.35 / 2.0, 1e-3);
  optimizer_tester.setSpeedLimit(0, true);
  auto [v_min4, v_max4] = optimizer_tester.getVelLimits();
  EXPECT_EQ(v_max4, 0.5f);
  EXPECT_EQ(v_min4, -0.35f);
  optimizer_tester.setSpeedLimit(0.75, false);
  auto [v_min5, v_max5] = optimizer_tester.getVelLimits();
  EXPECT_NEAR(v_max5, 0.75, 1e-3);
  EXPECT_NEAR(v_min5, -0.5249, 1e-2);
}

TEST(OptimizerTests, applyControlSequenceConstraintsTests)
{
  auto node = std::make_shared<nav2::LifecycleNode>("my_node");
  OptimizerTester optimizer_tester;
  node->declare_parameter("controller_frequency", rclcpp::ParameterValue(30.0));
  node->declare_parameter("mppic.batch_size", rclcpp::ParameterValue(1000));
  node->declare_parameter("mppic.time_steps", rclcpp::ParameterValue(50));
  node->declare_parameter("mppic.vx_max", rclcpp::ParameterValue(1.0));
  node->declare_parameter("mppic.vx_min", rclcpp::ParameterValue(-1.0));
  node->declare_parameter("mppic.vy_max", rclcpp::ParameterValue(0.75));
  node->declare_parameter("mppic.wz_max", rclcpp::ParameterValue(2.0));
  auto costmap_ros = std::make_shared<nav2_costmap_2d::Costmap2DROS>(
    "dummy_costmap", "", true);
  std::string name = "test";
  ParametersHandler param_handler(node, name);
  rclcpp_lifecycle::State lstate;
  costmap_ros->on_configure(lstate);
  auto tf_buffer = std::make_shared<tf2_ros::Buffer>(node->get_clock());
  optimizer_tester.initialize(node, "mppic", costmap_ros, tf_buffer, &param_handler);

  // Test constraints being applied to ensure feasibility of trajectories
  // Also tests param get of set vx/vy/wz min/maxes

  // Set model to omni to consider holonomic vy elements
  // Ack is not tested here because `applyConstraints` is covered in detail
  // in motion_models_test.cpp
  optimizer_tester.resetMotionModel();
  optimizer_tester.testSetOmniModel();
  auto & sequence = optimizer_tester.grabControlSequence();

  // Test boundary of limits
  sequence.vx = Eigen::ArrayXf::Ones(50);
  sequence.vy = 0.75 * Eigen::ArrayXf::Ones(50);
  sequence.wz = 2.0 * Eigen::ArrayXf::Ones(50);
  optimizer_tester.applyControlSequenceConstraintsWrapper();
  EXPECT_TRUE(sequence.vx.isApproxToConstant(1.0f));
  EXPECT_TRUE(sequence.vy.isApproxToConstant(0.75f));
  EXPECT_TRUE(sequence.wz.isApproxToConstant(2.0f));

  // Test breaking limits sets to maximum
  sequence.vx = 5.0 * Eigen::ArrayXf::Ones(50);
  sequence.vy = 5.0 * Eigen::ArrayXf::Ones(50);
  sequence.wz = 5.0 * Eigen::ArrayXf::Ones(50);
  optimizer_tester.applyControlSequenceConstraintsWrapper();
  EXPECT_TRUE(sequence.vx.isApproxToConstant(1.0f));
  EXPECT_TRUE(sequence.vy.isApproxToConstant(0.75f));
  EXPECT_TRUE(sequence.wz.isApproxToConstant(2.0f));

  // Test breaking limits sets to minimum
  sequence.vx = -5.0 * Eigen::ArrayXf::Ones(50);
  sequence.vy = -5.0 * Eigen::ArrayXf::Ones(50);
  sequence.wz = -5.0 * Eigen::ArrayXf::Ones(50);
  optimizer_tester.applyControlSequenceConstraintsWrapper();
  EXPECT_TRUE(sequence.vx.isApproxToConstant(-1.0f));
  EXPECT_TRUE(sequence.vy.isApproxToConstant(-0.75f));
  EXPECT_TRUE(sequence.wz.isApproxToConstant(-2.0f));
}

TEST(OptimizerTests, updateStateVelocitiesTests)
{
  auto node = std::make_shared<nav2::LifecycleNode>("my_node");
  OptimizerTester optimizer_tester;
  node->declare_parameter("controller_frequency", rclcpp::ParameterValue(30.0));
  node->declare_parameter("mppic.batch_size", rclcpp::ParameterValue(1000));
  node->declare_parameter("mppic.time_steps", rclcpp::ParameterValue(50));
  node->declare_parameter("mppic.vx_max", rclcpp::ParameterValue(1.0));
  node->declare_parameter("mppic.vx_min", rclcpp::ParameterValue(-1.0));
  node->declare_parameter("mppic.vy_max", rclcpp::ParameterValue(0.60));
  node->declare_parameter("mppic.wz_max", rclcpp::ParameterValue(2.0));
  node->declare_parameter("mppic.ax_max", rclcpp::ParameterValue(3.0));
  node->declare_parameter("mppic.ax_min", rclcpp::ParameterValue(-3.0));
  node->declare_parameter("mppic.ay_max", rclcpp::ParameterValue(3.0));
  node->declare_parameter("mppic.ay_min", rclcpp::ParameterValue(-3.0));
  node->declare_parameter("mppic.az_max", rclcpp::ParameterValue(3.5));
  auto costmap_ros = std::make_shared<nav2_costmap_2d::Costmap2DROS>(
    "dummy_costmap", "", true);
  std::string name = "test";
  ParametersHandler param_handler(node, name);
  rclcpp_lifecycle::State lstate;
  costmap_ros->on_configure(lstate);
  auto tf_buffer = std::make_shared<tf2_ros::Buffer>(node->get_clock());
  optimizer_tester.initialize(node, "mppic", costmap_ros, tf_buffer, &param_handler);

  // Test settings of the state to the initial robot speed to start rollout
  // Set model to omni to consider holonomic vy elements
  optimizer_tester.resetMotionModel();
  optimizer_tester.testSetOmniModel();
  optimizer_tester.testupdateStateVels();
}

TEST(OptimizerTests, getControlFromSequenceAsTwistTests)
{
  auto node = std::make_shared<nav2::LifecycleNode>("my_node");
  OptimizerTester optimizer_tester;
  node->declare_parameter("controller_frequency", rclcpp::ParameterValue(30.0));
  node->declare_parameter("mppic.batch_size", rclcpp::ParameterValue(1000));
  node->declare_parameter("mppic.time_steps", rclcpp::ParameterValue(50));
  node->declare_parameter("mppic.vx_max", rclcpp::ParameterValue(1.0));
  node->declare_parameter("mppic.vx_min", rclcpp::ParameterValue(-1.0));
  node->declare_parameter("mppic.vy_max", rclcpp::ParameterValue(0.60));
  node->declare_parameter("mppic.wz_max", rclcpp::ParameterValue(2.0));
  auto costmap_ros = std::make_shared<nav2_costmap_2d::Costmap2DROS>(
    "dummy_costmap", "", true);
  std::string name = "test";
  ParametersHandler param_handler(node, name);
  rclcpp_lifecycle::State lstate;
  costmap_ros->on_configure(lstate);
  auto tf_buffer = std::make_shared<tf2_ros::Buffer>(node->get_clock());
  optimizer_tester.initialize(node, "mppic", costmap_ros, tf_buffer, &param_handler);

  // Test conversion of control sequence into a Twist command to execute
  auto & sequence = optimizer_tester.grabControlSequence();
  sequence.vx = 0.25 * Eigen::ArrayXf::Ones(10);
  sequence.vy = 0.5 * Eigen::ArrayXf::Ones(10);
  sequence.wz = 0.1 * Eigen::ArrayXf::Ones(10);

  auto diff_t = optimizer_tester.getControlFromSequenceAsTwistWrapper();
  EXPECT_NEAR(diff_t.twist.linear.x, 0.25, 1e-6);
  EXPECT_NEAR(diff_t.twist.linear.y, 0.0, 1e-6);  // Y should not be populated
  EXPECT_NEAR(diff_t.twist.angular.z, 0.1, 1e-6);

  // Set model to omni to consider holonomic vy elements
  optimizer_tester.resetMotionModel();
  optimizer_tester.testSetOmniModel();
  auto omni_t = optimizer_tester.getControlFromSequenceAsTwistWrapper();
  EXPECT_NEAR(omni_t.twist.linear.x, 0.25, 1e-6);
  EXPECT_NEAR(omni_t.twist.linear.y, 0.5, 1e-6);  // Now it should be
  EXPECT_NEAR(omni_t.twist.angular.z, 0.1, 1e-6);
}

TEST(OptimizerTests, integrateStateVelocitiesTests)
{
  auto node = std::make_shared<nav2::LifecycleNode>("my_node");
  OptimizerTester optimizer_tester;
  node->declare_parameter("controller_frequency", rclcpp::ParameterValue(30.0));
  node->declare_parameter("mppic.batch_size", rclcpp::ParameterValue(1000));
  node->declare_parameter("mppic.model_dt", rclcpp::ParameterValue(0.1));
  node->declare_parameter("mppic.time_steps", rclcpp::ParameterValue(50));
  auto costmap_ros = std::make_shared<nav2_costmap_2d::Costmap2DROS>(
    "dummy_costmap", "", true);
  std::string name = "test";
  ParametersHandler param_handler(node, name);
  rclcpp_lifecycle::State lstate;
  costmap_ros->on_configure(lstate);
  auto tf_buffer = std::make_shared<tf2_ros::Buffer>(node->get_clock());
  optimizer_tester.initialize(node, "mppic", costmap_ros, tf_buffer, &param_handler);
  optimizer_tester.resetMotionModel();
  optimizer_tester.testSetOmniModel();

  // Test integration of velocities for trajectory rollout poses

  // Give it a couple of easy const traj and check rollout, start from 0
  models::State state;
  state.reset(1000, 50);
  models::Trajectories traj;
  traj.reset(1000, 50);
  state.vx = 0.1 * Eigen::ArrayXXf::Ones(1000, 50);
  state.vx.col(0) = Eigen::ArrayXf::Zero(1000);
  state.vy = Eigen::ArrayXXf::Zero(1000, 50);
  state.wz = Eigen::ArrayXXf::Zero(1000, 50);
  optimizer_tester.integrateStateVelocitiesWrapper(traj, state);
  EXPECT_TRUE(traj.y.isApproxToConstant(0.0f));
  EXPECT_TRUE(traj.yaws.isApproxToConstant(0.0f));
  for (unsigned int i = 0; i != traj.x.cols(); i++) {
    EXPECT_NEAR(traj.x(1, i), i * 0.1 /*vel*/ * 0.1 /*dt*/, 1e-3);
  }

  // Give it a bit of a more complex trajectory to crunch
  state.vy = 0.2 * Eigen::ArrayXXf::Ones(1000, 50);
  state.vy.col(0) = Eigen::ArrayXf::Zero(1000);
  optimizer_tester.integrateStateVelocitiesWrapper(traj, state);

  EXPECT_TRUE(traj.yaws.isApproxToConstant(0.0f));
  for (unsigned int i = 0; i != traj.x.cols(); i++) {
    EXPECT_NEAR(traj.x(1, i), i * 0.1 /*vel*/ * 0.1 /*dt*/, 1e-3);
    EXPECT_NEAR(traj.y(1, i), i * 0.2 /*vel*/ * 0.1 /*dt*/, 1e-3);
  }

  // Let's add some angular motion to the mix
  state.vy = Eigen::ArrayXXf::Zero(1000, 50);
  state.wz = 0.2 * Eigen::ArrayXXf::Ones(1000, 50);
  state.wz.col(0) = Eigen::ArrayXf::Zero(1000);
  optimizer_tester.integrateStateVelocitiesWrapper(traj, state);

  float x = 0;
  float y = 0;
  for (unsigned int i = 1; i != traj.x.cols(); i++) {
    x += (0.1 /*vx*/ * cosf(0.2 /*wz*/ * 0.1 /*model_dt*/ * (i - 1))) * 0.1 /*model_dt*/;
    y += (0.1 /*vx*/ * sinf(0.2 /*wz*/ * 0.1 /*model_dt*/ * (i - 1))) * 0.1 /*model_dt*/;

    EXPECT_NEAR(traj.x(1, i), x, 1e-6);
    EXPECT_NEAR(traj.y(1, i), y, 1e-6);
  }
}

TEST(OptimizerTests, TestGetters)
{
  OptimizerTester optimizer_tester;
  optimizer_tester.fillOptimizerWithGarbage();

  auto control_seq = optimizer_tester.getOptimalControlSequence();
  EXPECT_EQ(control_seq.vx(0), 342.0);
  EXPECT_EQ(control_seq.vx.rows(), 30);

  auto node = std::make_shared<nav2::LifecycleNode>("my_node");
  node->declare_parameter("controller_frequency", rclcpp::ParameterValue(30.0));
  auto costmap_ros = std::make_shared<nav2_costmap_2d::Costmap2DROS>(
    "dummy_costmap", "", true);
  std::string test = "test";
  ParametersHandler param_handler(node, test);
  rclcpp_lifecycle::State lstate;
  costmap_ros->on_configure(lstate);
  auto tf_buffer = std::make_shared<tf2_ros::Buffer>(node->get_clock());
  optimizer_tester.initialize(node, "mppic", costmap_ros, tf_buffer, &param_handler);
  EXPECT_EQ(optimizer_tester.getSettings().model_dt, 0.05f);
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);

  rclcpp::init(0, nullptr);

  int result = RUN_ALL_TESTS();

  rclcpp::shutdown();

  return result;
}
