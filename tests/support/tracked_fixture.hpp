//
// Created by buidangnguyen05 on 10/27/26.
//

#pragma once

#include "tracked.hpp"

#include <gtest/gtest.h>

class TrackedFixture : public ::testing::Test {
protected:
  TrackedFixture() { stl::test::Tracked::reset(); }

  ~TrackedFixture() override {
    EXPECT_EQ(stl::test::Tracked::live(), 0) << "Details: " << stl::test::Tracked::snapshot();
  }
};
