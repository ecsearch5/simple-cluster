/*
 *  SIMPLE CLUSTERS: A simple library for clustering works.
 *  Copyright (C) 2014 Nguyen Anh Tuan <t_nguyen@hal.t.u-tokyo.ac.jp>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  test_utilities.cpp
 *
 *  Created on: 2014/09/09
 *      Author: Nguyen Anh Tuan <t_nguyen@hal.t.u-tokyo.ac.jp>
 */

#include <iostream>
#include <vector>
#include <random>
#include <gtest/gtest.h>
#include <math.h>
#include <stdlib.h>
#include "utilities.h"

using namespace std;
using namespace SimpleCluster;

/**
 * A comparator
 */
int compare_float(const float * _a, const float * _b) {
	if(*_a > *_b) return 1;
	else if(*_a < *_b) return -1;
	else return 0;
}

/**
 * Customized test case for testing
 */
class UtilTest : public ::testing::Test {
protected:
	// Per-test-case set-up.
	// Called before the first test in this test case.
	// Can be omitted if not needed.
	static void SetUpTestCase() {
		N = 10000;
		d = 128;
		size_t i, j;

		// For generating random numbers
		random_device rd;
		mt19937 gen(rd());
		uniform_real_distribution<float> real_dis(0.0, static_cast<float>(N));

		data = (float **)::operator new(N * sizeof(float *));
		for(i = 0; i < N; i++) {
			::new(data +i) float *;
			data[i] = (float *)::operator new(d * sizeof(float));
			for(j = 0; j < d; j++) {
				::new(data[i] + j) float;
				data[i][j] = real_dis(gen);
			}
		}
	}

	// Per-test-case tear-down.
	// Called after the last test in this test case.
	// Can be omitted if not needed.
	static void TearDownTestCase() {
		size_t i;
		for(i = 0; i < N; i++)
			::delete data[i];
		::delete data;
	}

	// You can define per-test set-up and tear-down logic as usual.
	virtual void SetUp() { }
	virtual void TearDown() {}

public:
	// Some expensive resource shared by all tests.
	static float ** data;
	static size_t N;
	static size_t d;
};

float ** UtilTest::data;
size_t UtilTest::N;
size_t UtilTest::d;

// Let's start with some testcases
TEST_F(UtilTest, test1) {
	EXPECT_EQ(0.0, distance(data[0],data[0],d));
}

TEST_F(UtilTest, test2) {
	EXPECT_EQ(0.0, distance_square(data[0],data[0],d));
}

TEST_F(UtilTest, test3) {
	EXPECT_LT(0.0, distance(data[0],data[1],d));
}

TEST_F(UtilTest, test5) {
	size_t index[] = {0,1,2,3,4};
	size_t * index2 = (size_t *)malloc(5 * sizeof(size_t));
	for(size_t i = 0; i < 5; i++)
		index2[i] = i;

	float * tmp = (float *)calloc(d,sizeof(float));
	float * v1 = mean_vector(data,index,tmp,d,5);
	float * v2 = mean_vector(data,index2,tmp,d,5);
	EXPECT_EQ(0.0, distance(v1,v2,d));

	free(index2);
	free(tmp);
	free(v1);
	free(v2);
}

TEST_F(UtilTest, test6) {
	float arr[] = {1.0, 3.0, 5.0, 7.0, 9.0};
	float m = quick_select_k(arr,5,3,compare_float);
	EXPECT_EQ(7.0,m);
}

TEST_F(UtilTest, test7) {
	float arr[10000];
	for(int i = 0; i < 10000; i++)
		arr[i] = i;
	float m = quick_select_k(arr,10000,5000,compare_float);
	EXPECT_EQ(5000.0,m);
}

TEST_F(UtilTest, test8) {
	float * t;
	EXPECT_TRUE(init_array<float>(t,1000) && t!=nullptr);
}

TEST_F(UtilTest, test9) {
	float * t;
	EXPECT_TRUE(init_array<float>(t,d) && t != nullptr);
	EXPECT_TRUE(copy_array<float>(data[0],t,d));
	for(size_t i = 0; i < d; i++) {
		EXPECT_TRUE(data[0][i] == t[i]);
	}
}

TEST_F(UtilTest, test10) {
	float ** t;
	EXPECT_TRUE(init_array_2<float>(t,1000,200) && t!=nullptr);
}

TEST_F(UtilTest, test11) {
	float ** t;
	EXPECT_TRUE(init_array_2<float>(t,N,d) && t != nullptr);
	EXPECT_TRUE(copy_array_2<float>(data,t,N,d));
	for(size_t i = 0; i < N; i++) {
		for(size_t j = 0; j < d; j++) {
			EXPECT_TRUE(data[i][j] == t[i][j]);
		}
	}
}

TEST_F(UtilTest, test12) {
	float ** t;
	EXPECT_TRUE(init_array_2<float>(t,N,d) && t != nullptr);
	EXPECT_TRUE(dealloc_array_2<float>(t,N));
}
