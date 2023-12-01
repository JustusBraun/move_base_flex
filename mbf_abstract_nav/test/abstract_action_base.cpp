#include <gmock/gmock.h>
#include <gtest/gtest.h>

// dummy message
#include <mbf_msgs/action/get_path.hpp>
#include <mbf_utility/robot_information.h>

#include <mbf_abstract_nav/abstract_action_base.hpp>
#include <mbf_abstract_nav/abstract_execution_base.h>

using namespace mbf_abstract_nav;

// mocked version of an execution
struct MockedExecution : public AbstractExecutionBase {
  typedef boost::shared_ptr<MockedExecution> Ptr;

  MockedExecution(const mbf_utility::RobotInformation& ri, const rclcpp::Node::SharedPtr& node) : AbstractExecutionBase("mocked_execution", ri, node) {}

  MOCK_METHOD0(cancel, bool());

protected:
  MOCK_METHOD0(run, void());
};

using testing::Test;

// fixture with access to the AbstractActionBase's internals
struct AbstractActionBaseFixture
    : public AbstractActionBase<mbf_msgs::action::GetPath, MockedExecution>,
      public Test {
  // required members for the c'tor
  std::string test_name;
  rclcpp::Node::SharedPtr node_;
  TF tf_;
  mbf_utility::RobotInformation ri;

  AbstractActionBaseFixture()
      : test_name("action_base"),
        node_(std::make_shared<rclcpp::Node>("test_node")),
        tf_(node_->get_clock()),
        ri(node_, tf_, "global_frame", "local_frame", rclcpp::Duration(0,0)),
        AbstractActionBase(node_, test_name, ri)
  {
  }

  void runImpl(GoalHandle &goal_handle, MockedExecution &execution) {}
};

TEST_F(AbstractActionBaseFixture, thread_stop)
{
  unsigned char slot = 1;
  concurrency_slots_[slot].execution.reset(new MockedExecution(AbstractActionBaseFixture::ri, node_));
  concurrency_slots_[slot].thread_ptr =
      threads_.create_thread(boost::bind(&AbstractActionBaseFixture::run, this,
                                         boost::ref(concurrency_slots_[slot])));
}

using testing::Return;

TEST_F(AbstractActionBaseFixture, cancelAll)
{
  // spawn a bunch of threads
  for (unsigned char slot = 0; slot != 10; ++slot) {
    concurrency_slots_[slot].execution.reset(new MockedExecution(AbstractActionBaseFixture::ri, node_));
    // set the expectation
    EXPECT_CALL(*concurrency_slots_[slot].execution, cancel())
        .WillRepeatedly(Return(true));

    // set the in_use flag --> this should turn to false
    concurrency_slots_[slot].in_use = true;
    concurrency_slots_[slot].thread_ptr = threads_.create_thread(
        boost::bind(&AbstractActionBaseFixture::run, this,
                    boost::ref(concurrency_slots_[slot])));
  }

  // cancel all of slots
  cancelAll();

  // check the result
  for (ConcurrencyMap::iterator slot = concurrency_slots_.begin();
       slot != concurrency_slots_.end(); ++slot)
    ASSERT_FALSE(slot->second.in_use);
}

int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}