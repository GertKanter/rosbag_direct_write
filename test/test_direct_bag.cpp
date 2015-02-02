#include <gtest/gtest.h>

// I know this is quite strange, but I need to get to the internals of the
// rosbag::Bag class to do some easy checking.
#include <geometry_msgs/PointStamped.h>
#define private public
#define protected public
#include <rosbag/bag.h>
#undef private
#undef protected
#include <rosbag/view.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/Imu.h>
#include <std_msgs/Header.h>

#include "rosbag_direct_write/direct_bag.h"

#include "custom_image.h"
#include "custom_imu.h"

TEST(DirectBagTestSuite, test_record_bag)
{
  std::string bag_name = "test_direct.bag";
  // Write a bag with ROS messages
  rosbag_direct_write::DirectBag bag(bag_name);
  size_t number_of_iterations = 3;
  size_t width = 1024, height = 768;
  sensor_msgs::Image image;
  image.header.stamp.fromSec(ros::WallTime::now().toSec());
  image.header.frame_id = "/camera_frame";
  image.height = height;
  image.width = width;
  image.encoding = "Encoding 1";
  image.is_bigendian = true;
  image.step = 1;
  image.data = std::vector<uint8_t>(width * height, 0x12);
  for (size_t i = 0; i < number_of_iterations; ++i)
  {
    image.header.stamp.fromSec(ros::WallTime::now().toSec());

    bag.write("camera", image.header.stamp, image);
  }
  bag.close();
  // Read bag with normal rosbag interface
  rosbag::Bag ros_bag;
  ros_bag.open(bag_name, rosbag::bagmode::Read);
  std::vector<std::string> topics;
  topics.push_back(std::string("camera"));
  rosbag::View view(ros_bag, rosbag::TopicQuery(topics));
  size_t number_of_images = 0;
  for (auto &m : view)
  {
    ASSERT_STREQ("camera", m.getTopic().c_str());
    auto msg = m.instantiate<sensor_msgs::Image>();
    ASSERT_NE(nullptr, msg);
    ASSERT_STREQ("/camera_frame", msg->header.frame_id.c_str());
    ASSERT_EQ(width, msg->width);
    ASSERT_EQ(height, msg->height);
    ASSERT_STREQ("Encoding 1", msg->encoding.c_str());
    ASSERT_TRUE(msg->is_bigendian);
    ASSERT_EQ(1, msg->step);
    ASSERT_EQ(width * height, msg->data.size());
    for (auto &e : msg->data)
    {
      ASSERT_EQ(0x12, e);
    }
    number_of_images += 1;
  }
  ASSERT_EQ(number_of_iterations, number_of_images);
  ros_bag.close();
}

TEST(DirectBagTestSuite, test_record_bag_small_messages)
{
  std::string bag_name = "test_direct_small_messages.bag";
  // Write a bag with many small ROS messages
  // Explicitly set the chunk size to 768kb
  rosbag_direct_write::DirectBag bag(bag_name, 768 * 1024);
  size_t number_of_iterations = 2500;  // This is enough to make two chunks
  sensor_msgs::Imu imu;
  imu.header.stamp.fromSec(ros::WallTime::now().toSec());
  imu.header.frame_id = "/imu_frame";
  for (size_t i = 0; i < number_of_iterations; ++i)
  {
    imu.header.stamp.fromSec(ros::WallTime::now().toSec());

    bag.write("imu", imu.header.stamp, imu);
  }
  bag.close();
  // Read bag with normal rosbag interface
  rosbag::Bag ros_bag;
  ros_bag.open(bag_name, rosbag::bagmode::Read);
  ASSERT_EQ(ros_bag.chunk_count_, 2);
  std::vector<std::string> topics;
  topics.push_back(std::string("imu"));
  rosbag::View view(ros_bag, rosbag::TopicQuery(topics));
  size_t number_of_imu_messages = 0;
  for (auto &m : view)
  {
    ASSERT_STREQ("imu", m.getTopic().c_str());
    auto msg = m.instantiate<sensor_msgs::Imu>();
    ASSERT_NE(nullptr, msg);
    ASSERT_STREQ("/imu_frame", msg->header.frame_id.c_str());
    number_of_imu_messages += 1;
  }
  ASSERT_EQ(number_of_iterations, number_of_imu_messages);
  ros_bag.close();
}

TEST(DirectBagTestSuite, test_record_bag_mixed)
{
  std::string bag_name = "test_direct_mixed.bag";
  // Write a bag with many small ROS messages
  // Explicitly set the chunk size to 768kb
  rosbag_direct_write::DirectBag bag(bag_name, 768 * 1024);
  size_t number_of_iterations = 250;
  size_t number_of_imu_per_iteration = 10;
  __custom_image image;
  image.stamp.fromSec(ros::WallTime::now().toSec());
  image.data = rosbag_direct_write::VectorBuffer(640 * 480, 0x12);
  __custom_imu imu;
  imu.stamp.fromSec(ros::WallTime::now().toSec());
  imu.x = 1.0;
  imu.y = 2.0;
  imu.z = 3.0;
  for (size_t i = 0; i < number_of_iterations; ++i)
  {
    for (size_t j = 0; j < number_of_imu_per_iteration; ++j)
    {
      imu.stamp.fromSec(ros::WallTime::now().toSec());

      bag.write("point_stamped", imu.stamp, imu);
    }
    image.stamp.fromSec(ros::WallTime::now().toSec());

    bag.write("camera", image.stamp, image);
  }
  bag.close();
  // Read bag with normal rosbag interface
  rosbag::Bag ros_bag;
  ros_bag.open(bag_name, rosbag::bagmode::Read);
  std::vector<std::string> topics;
  topics.push_back(std::string("point_stamped"));
  topics.push_back(std::string("camera"));
  rosbag::View view(ros_bag, rosbag::TopicQuery(topics));
  size_t number_of_imu_messages = 0;
  size_t number_of_image_messages = 0;
  for (auto &m : view)
  {
    auto image_msg = m.instantiate<sensor_msgs::Image>();
    auto imu_msg = m.instantiate<geometry_msgs::PointStamped>();
    ASSERT_TRUE(image_msg != nullptr || imu_msg != nullptr);
    if (image_msg != nullptr)
    {
      number_of_image_messages += 1;
    }
    if (imu_msg != nullptr)
    {
      ASSERT_EQ(imu_msg->point.x, 1);
      ASSERT_EQ(imu_msg->point.y, 2);
      ASSERT_EQ(imu_msg->point.z, 3);
      number_of_imu_messages += 1;
    }
  }
  ASSERT_EQ(number_of_iterations * number_of_imu_per_iteration,
            number_of_imu_messages);
  ASSERT_EQ(number_of_iterations, number_of_image_messages);
  ros_bag.close();
}

int main(int argc, char **argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
