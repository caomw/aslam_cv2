#include <algorithm>
#include <eigen-checks/gtest.h>
#include <glog/logging.h>
#include <gtest/gtest.h>
#include <math.h>
#include <vector>

#include <aslam/cameras/camera-pinhole.h>
#include <aslam/common/entrypoint.h>
#include <aslam/common/pose-types.h>
#include <aslam/frames/visual-frame.h>
#include <aslam/matcher/match.h>
#include <aslam/matcher/matching-engine-non-exclusive.h>
#include <aslam/matcher/matching-problem-frame-to-frame.h>

class MatcherTest : public testing::Test {
 protected:
  virtual void SetUp() {
    camera_ = aslam::PinholeCamera::createTestCamera();
    apple_frame_ = aslam::VisualFrame::createEmptyTestVisualFrame(camera_, 0);
    banana_frame_ = aslam::VisualFrame::createEmptyTestVisualFrame(camera_, 1);

    aslam::Quaternion q_apple_banana;
    q_apple_banana.setIdentity();

    image_space_distance_threshold_ = 25.0;
    hamming_distance_threshold_ = 60;


  }

  double image_space_distance_threshold_;
  int hamming_distance_threshold_;

  aslam::VisualFrame::Ptr apple_frame_;
  aslam::VisualFrame::Ptr banana_frame_;

  aslam::MatchingEngineNonExclusive<aslam::MatchingProblemFrameToFrame> matching_engine_;

  aslam::PinholeCamera::Ptr camera_;
};

TEST_F(MatcherTest, EmptyMatch) {
  aslam::MatchingEngineNonExclusive<aslam::MatchingProblemFrameToFrame> matching_engine;

  aslam::MatchingProblemFrameToFrame::Ptr matching_problem =
      aslam::aligned_shared<aslam::MatchingProblemFrameToFrame>(apple_frame_,
                                                   banana_frame_,
                                                   Eigen::Matrix3d::Identity(),
                                                   image_space_distance_threshold_,
                                                   hamming_distance_threshold_);
  aslam::Matches matches;
  matching_engine_.match(matching_problem.get(), &matches);

  EXPECT_TRUE(matches.empty());
}

TEST_F(MatcherTest, MatchIdentity) {
  Eigen::Matrix2Xd apple_keypoints = Eigen::Matrix2Xd::Zero(2, 1);
  Eigen::Matrix2Xd banana_keypoints = Eigen::Matrix2Xd::Zero(2, 1);

  Eigen::Matrix<unsigned char, 48, 1> apple_descriptors = Eigen::Matrix<unsigned char, 48, 1>::Zero();
  Eigen::Matrix<unsigned char, 48, 1> banana_descriptors = Eigen::Matrix<unsigned char, 48, 1>::Zero();

  apple_frame_->setKeypointMeasurements(apple_keypoints);
  apple_frame_->setDescriptors(apple_descriptors);

  banana_frame_->setKeypointMeasurements(apple_keypoints);
  banana_frame_->setDescriptors(banana_descriptors);

  Eigen::Matrix<unsigned char, Eigen::Dynamic, Eigen::Dynamic> bla = banana_frame_->getDescriptors();
  std::cout << "COLS: " << bla.cols() << std::endl;

  aslam::MatchingProblemFrameToFrame::Ptr matching_problem =
      aslam::aligned_shared<aslam::MatchingProblemFrameToFrame>(apple_frame_,
                                                   banana_frame_,
                                                   Eigen::Matrix3d::Identity(),
                                                   image_space_distance_threshold_,
                                                   hamming_distance_threshold_);
  aslam::Matches matches;
  matching_engine_.match(matching_problem.get(), &matches);

  ASSERT_EQ(1, matches.size());
  aslam::Match match = matches[0];
  EXPECT_EQ(0, match.index_apple);
  EXPECT_EQ(0, match.index_banana);
  EXPECT_DOUBLE_EQ(1.0, match.score);
}

TEST_F(MatcherTest, MatchRotation) {
  size_t image_height = camera_->imageHeight();
  size_t image_width = camera_->imageWidth();

  Eigen::Matrix2Xd apple_keypoints = Eigen::Matrix2Xd::Zero(2, 1);
  apple_keypoints(0,0) =  static_cast<double>(image_width) / 2.0;
  apple_keypoints(1,0) =  static_cast<double>(image_height) / 2.0;
  Eigen::Vector3d apple_ray;

  camera_->backProject3(apple_keypoints.col(0), &apple_ray);

  EXPECT_TRUE(EIGEN_MATRIX_NEAR(apple_ray, Eigen::Vector3d(0.0, 0.0, 1.0), 1e-12));

  Eigen::Vector3d axis_angle(0.0, 0.3, 0.0);
  aslam::Quaternion q_apple_banana(axis_angle);

  Eigen::Matrix3d C_apple_banana = q_apple_banana.getRotationMatrix();
  Eigen::Matrix3d C_banana_apple = C_apple_banana.transpose();

  Eigen::Vector3d banana_ray = C_banana_apple * apple_ray;

  Eigen::Vector3d apple_ray_banana = C_apple_banana * banana_ray;

  Eigen::Vector2d banana_keypoint;
  camera_->project3(banana_ray, &banana_keypoint);

  Eigen::Matrix2Xd banana_keypoints = Eigen::Matrix2Xd::Zero(2, 1);
  banana_keypoints.col(0) = banana_keypoint;

  Eigen::Matrix<unsigned char, 48, 1> apple_descriptors = Eigen::Matrix<unsigned char, 48, 1>::Zero();
  Eigen::Matrix<unsigned char, 48, 1> banana_descriptors = Eigen::Matrix<unsigned char, 48, 1>::Zero();

  apple_frame_->setKeypointMeasurements(apple_keypoints);
  apple_frame_->setDescriptors(apple_descriptors);

  banana_frame_->setKeypointMeasurements(banana_keypoints);
  banana_frame_->setDescriptors(banana_descriptors);

  aslam::MatchingProblemFrameToFrame::Ptr matching_problem =
      aslam::aligned_shared<aslam::MatchingProblemFrameToFrame>(apple_frame_,
                                                   banana_frame_,
                                                   C_apple_banana,
                                                   image_space_distance_threshold_,
                                                   hamming_distance_threshold_);

  aslam::Matches matches;
  matching_engine_.match(matching_problem.get(), &matches);

  ASSERT_EQ(1, matches.size());
  aslam::Match match = matches[0];
  EXPECT_EQ(0, match.index_apple);
  EXPECT_EQ(0, match.index_banana);
  EXPECT_DOUBLE_EQ(1.0, match.score);
}

TEST_F(MatcherTest, TestImageSpaceBorderOut) {
  size_t image_height = camera_->imageHeight();
  size_t image_width = camera_->imageWidth();

  Eigen::Matrix2Xd apple_keypoints = Eigen::Matrix2Xd::Constant(2, 1, 20.0);
  Eigen::Matrix2Xd banana_keypoints = Eigen::Matrix2Xd::Constant(2, 1, 20.0);
  banana_keypoints(0, 0) = 20.0 + image_space_distance_threshold_;

  Eigen::Matrix<unsigned char, 48, 1> apple_descriptors = Eigen::Matrix<unsigned char, 48, 1>::Zero();
  Eigen::Matrix<unsigned char, 48, 1> banana_descriptors = Eigen::Matrix<unsigned char, 48, 1>::Zero();

  apple_frame_->setKeypointMeasurements(apple_keypoints);
  apple_frame_->setDescriptors(apple_descriptors);

  banana_frame_->setKeypointMeasurements(banana_keypoints);
  banana_frame_->setDescriptors(banana_descriptors);

  aslam::MatchingProblemFrameToFrame::Ptr matching_problem =
      aslam::aligned_shared<aslam::MatchingProblemFrameToFrame>(apple_frame_,
                                                   banana_frame_,
                                                   Eigen::Matrix3d::Identity(),
                                                   image_space_distance_threshold_,
                                                   hamming_distance_threshold_);
  aslam::Matches matches;
  matching_engine_.match(matching_problem.get(), &matches);

  EXPECT_TRUE(matches.empty());
}

TEST_F(MatcherTest, TestImageSpaceBorderIn) {
  Eigen::Matrix2Xd apple_keypoints = Eigen::Matrix2Xd::Constant(2, 1, 20.0);
  Eigen::Matrix2Xd banana_keypoints = Eigen::Matrix2Xd::Constant(2, 1, 20.0);
  banana_keypoints(0, 0) = 20.0 + image_space_distance_threshold_ - 1e-12;

  Eigen::Matrix<unsigned char, 48, 1> apple_descriptors = Eigen::Matrix<unsigned char, 48, 1>::Zero();
  Eigen::Matrix<unsigned char, 48, 1> banana_descriptors = Eigen::Matrix<unsigned char, 48, 1>::Zero();

  apple_frame_->setKeypointMeasurements(apple_keypoints);
  apple_frame_->setDescriptors(apple_descriptors);

  banana_frame_->setKeypointMeasurements(banana_keypoints);
  banana_frame_->setDescriptors(banana_descriptors);

  aslam::MatchingProblemFrameToFrame::Ptr matching_problem =
      aslam::aligned_shared<aslam::MatchingProblemFrameToFrame>(apple_frame_,
                                                   banana_frame_,
                                                   Eigen::Matrix3d::Identity(),
                                                   image_space_distance_threshold_,
                                                   hamming_distance_threshold_);

  aslam::Matches matches;
  matching_engine_.match(matching_problem.get(), &matches);

  EXPECT_EQ(1, matches.size());
}

TEST_F(MatcherTest, TestComplex) {
  size_t num_apples = 5;
  size_t num_bananas = 5;
  Eigen::Matrix2Xd apple_keypoints = Eigen::Matrix2Xd::Zero(2, num_apples);
  Eigen::Matrix2Xd banana_keypoints = Eigen::Matrix2Xd::Zero(2, num_bananas);

  apple_keypoints(0, 0) = 290.0;
  apple_keypoints(1, 0) = 200.0;

  apple_keypoints(0, 1) = 300.0;
  apple_keypoints(1, 1) = 200.0;

  apple_keypoints(0, 2) = 155.0;
  apple_keypoints(1, 2) = 200.0;

  apple_keypoints(0, 3) = 80.0;
  apple_keypoints(1, 3) = 120.0;

  apple_keypoints(0, 4) = 600.0;
  apple_keypoints(1, 4) = 400.0;

  Eigen::Vector3d axis_angle(0.1, 0.1, 0.2);
  aslam::Quaternion q_apple_banana(axis_angle);

  Eigen::Matrix3d C_apple_banana = q_apple_banana.getRotationMatrix();
  Eigen::Matrix3d C_banana_apple = C_apple_banana.transpose();


  // Transform points into banana frame.
  Eigen::Matrix3Xd rays_apple = Eigen::Matrix3Xd::Zero(3, 5);

  for (size_t apple_idx = 0; apple_idx < num_apples; ++apple_idx) {
    Eigen::Vector3d apple_ray;
    ASSERT_TRUE(camera_->backProject3(apple_keypoints.col(apple_idx), &apple_ray));
    rays_apple.col(apple_idx) = apple_ray;
  }

  Eigen::Matrix3Xd banana_rays_apple = C_banana_apple * rays_apple;

  size_t banana_idx = 0;
  for (size_t apple_idx = 0; apple_idx < num_apples; ++apple_idx) {
    Eigen::Vector2d banana_keypoint;
    aslam::ProjectionResult result = camera_->project3(banana_rays_apple.col(apple_idx), &banana_keypoint);
    if (result.isKeypointVisible()) banana_keypoints.col(banana_idx++) = banana_keypoint;
  }
  CHECK_EQ(banana_idx, 5);

  Eigen::Matrix<unsigned char, 48, 5> apple_descriptors = Eigen::Matrix<unsigned char, 48, 5>::Zero();
  Eigen::Matrix<unsigned char, 48, 5> banana_descriptors = Eigen::Matrix<unsigned char, 48, 5>::Zero();

  apple_frame_->setKeypointMeasurements(apple_keypoints);
  apple_frame_->setDescriptors(apple_descriptors);

  banana_frame_->setKeypointMeasurements(banana_keypoints);
  banana_frame_->setDescriptors(banana_descriptors);

  aslam::MatchingProblemFrameToFrame::Ptr matching_problem =
      aslam::aligned_shared<aslam::MatchingProblemFrameToFrame>(apple_frame_,
                                                   banana_frame_,
                                                   C_apple_banana,
                                                   image_space_distance_threshold_,
                                                   hamming_distance_threshold_);

  aslam::Matches matches;
  matching_engine_.match(matching_problem.get(), &matches);

  aslam::Matches ground_truth_matches;
  // The ground truth matches: keypoint 0 and 1 are within image_space distance!
  ground_truth_matches.emplace_back(0, 0, 1.0);
  ground_truth_matches.emplace_back(1, 0, 1.0);
  ground_truth_matches.emplace_back(0, 1, 1.0);
  ground_truth_matches.emplace_back(1, 1, 1.0);
  ground_truth_matches.emplace_back(2, 2, 1.0);
  ground_truth_matches.emplace_back(3, 3, 1.0);
  ground_truth_matches.emplace_back(4, 4, 1.0);

  size_t num_matches = matches.size();
  EXPECT_EQ(ground_truth_matches.size(), num_matches);
  for (size_t i = 0; i < num_matches; ++i) {
    EXPECT_EQ(matches[i], ground_truth_matches[i]);
  }
}

ASLAM_UNITTEST_ENTRYPOINT
