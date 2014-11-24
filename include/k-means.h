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
 *  k-means.h
 *
 *  Created on: 2014/09/04
 *      Author: Nguyen Anh Tuan <t_nguyen@hal.t.u-tokyo.ac.jp>
 */

#ifndef K_MEANS_H_
#define K_MEANS_H_

#include <iostream>
#include <exception>
#include <algorithm>
#include <vector>
#include <random>
#include <cstring>
#include <climits>
#include <cstdlib>
#include <cfloat>
#include <cmath>
#include "utilities.h"
#include "kd-tree.h"

#ifdef _OPENMP
#include <omp.h>
#define SET_THREAD_NUM omp_set_num_threads(n_thread)
#else
#define SET_THREAD_NUM 0 // disable multi-thread
#endif

#ifndef FLT_MAX
#define FLT_MAX 3.40282346638528859812e+38F
#endif

using namespace std;

/**
 * Cluster methods' space
 */
namespace SimpleCluster {

/**
 * Types of assigning methods
 */
enum class KmeansAssignType {
	LINEAR,
	NN_KD_TREE,
	ANN_KD_TREE
};

/**
 * Types of the k-means seeding
 */
enum class KmeansType {
	RANDOM_SEEDS, // randomly generated seeds
	KMEANS_PLUS_SEEDS, // k-means++
	USER_SEEDS // take the seeds from input
};

/**
 * Criteria
 */
typedef struct {
	float alpha;
	float accuracy;
	int iterations;
} KmeansCriteria;

/**
 * Create random seeds for k-means
 * @param data input data
 * @param seeds the seeds
 * @param d the dimensions of the data
 * @param N the number of the data
 * @param k the number of clusters
 * @param n_thread the number of threads
 * @param verbose for debugging
 */
template<typename DataType>
void random_seeds(
		DataType ** data,
		float **& seeds,
		int d,
		int N,
		int k,
		int n_thread,
		bool verbose) {
	int i;
	int j;
#ifdef _WIN32
	int * tmp;
	SimpleCluster::init_array<int>(tmp,k);
#else
	int tmp[k];
#endif

	// For generating random numbers
	random_device rd;
	mt19937 gen(rd());

	for(i = 0; i < k; i++)
		tmp[i] = static_cast<int>(i);
#ifdef _OPENMP
	SET_THREAD_NUM;
#pragma omp parallel
	{
#pragma omp for private(i)
#endif
		for(i = k; i < N; i++) {
			uniform_int_distribution<> int_dis(i,N-1);
			j = int_dis(gen);
			if(j < k)
				tmp[j] = static_cast<int>(i);
		}
#ifdef _OPENMP
	}
#endif

	for(i = 0; i < k; i++) {
		for(j = 0; j < d; j++) {
			seeds[i][j] = static_cast<float>(data[tmp[i]][j]);
		}
	}
}

/**
 * Create seeds for k-means++
 * @param d the dimensions of the data
 * @param N the number of the data
 * @param k the number of clusters
 * @param data input data
 * @param seeds the seeds
 * @param n_thread the number of threads
 * @param d_type the type of distance. Available options are NORM_L1, NORM_L2, HAMMING
 * @param verbose for debugging
 */
template<typename DataType>
void kmeans_pp_seeds(
		DataType ** data,
		float **& seeds,
		DistanceType d_type,
		int d,
		int N,
		int k,
		int n_thread,
		bool verbose) {
	// For generating random numbers
	random_device rd;
	mt19937 gen(rd());

	uniform_int_distribution<int> int_dis(0, N - 1);
	int tmp = int_dis(gen);

	DataType * d_tmp;
	if(!init_array<DataType>(d_tmp,d)) {
		cerr << "d_tmp: Cannot allocate the memory" << endl;
		exit(1);
	}

	//	copy_array<DataType>(data[tmp],seeds[0],d);
	for(int j = 0; j < d; j++) {
		seeds[0][j] = static_cast<float>(data[tmp][j]);
	}
	float * distances;
	init_array<float>(distances,N);
	float * sum_distances;
	init_array<float>(sum_distances,N);

	int i;
#ifdef _OPENMP
	SET_THREAD_NUM;
#pragma omp parallel
	{
#pragma omp for private(i)
#endif
		for(i = 0; i < N; i++) {
			if(d_type == DistanceType::NORM_L2)
				distances[i] = distance_l2_square<DataType>(data[i], d_tmp, d);
			else if(d_type == DistanceType::NORM_L1)
				distances[i] = distance_l1<DataType>(data[i], d_tmp, d);
			sum_distances[i] = 0.0;
		}
#ifdef _OPENMP
	}
#endif

	float sum, tmp2 = 0.0, sum1, sum2, pivot;
	int count = 1, j, t;
	for(count = 1; count < k; count++) {
		sum = 0.0;
		for(i = 0; i < N; i++) {
			sum += distances[i];
			sum_distances[i] = sum;
		}
		uniform_real_distribution<float> real_dis(0, sum);
		pivot = real_dis(gen);

		for(i = 0; i < N - 1; i++) {
			sum1 = sum_distances[i];
			sum2 = sum_distances[i + 1];
			if(sum1 < pivot && pivot <= sum2)
				break;
		}
		j = (i + 1) % N;
		for(t = 0; t < d; t++) {
			seeds[count][t] = static_cast<float>(data[j][t]);
		}
		// Update the distances
		if(count < k) {
			for(i = 0; i < N; i++) {
				if(d_type == DistanceType::NORM_L2)
					tmp2 = distance_l2_square<DataType>(d_tmp,data[i],d);
				else if(d_type == DistanceType::NORM_L1)
					tmp2 = distance_l1<DataType>(d_tmp,data[i],d);
				if(distances[i] > tmp2) distances[i] = tmp2;
			}
		}
	}
}

/**
 * After having a set of centers,
 * we need to assign data into each cluster respectively.
 * This solution uses linear search to assign data.
 * @param d the dimensions of the data
 * @param N the number of the data
 * @param k the number of clusters
 * @param data input data
 * @param centers the centers
 * @param clusters the clusters
 * @param d_type the type of distance. Available options are NORM_L1, NORM_L2, HAMMING
 * @param n_thread the number of threads
 * @param verbose for debugging
 */
template<typename DataType>
void linear_assign(
		DataType ** data,
		DataType ** centers,
		vector<i_vector>& clusters,
		DistanceType d_type,
		int d,
		int N,
		int k,
		int n_thread,
		bool verbose) {
	int i;
	int tmp;

	double min = 0.0;
#ifdef _OPENMP
	SET_THREAD_NUM;
#pragma omp parallel
	{
#pragma omp for private(i)
#endif
		for(i = 0; i < N; i++) {
			// Find the minimum distances between d_tmp and a centroid
			linear_search<DataType>(centers,data[i],d_type,tmp,min,k,d,verbose);
			// Assign the data[i] into cluster tmp
			clusters[tmp].push_back(static_cast<int>(i));
		}
#ifdef _OPENMP
	}
#endif
}

/**
 * After having a set of centers,
 * we need to assign data into each cluster respectively.
 * This solution uses kd-tree search to assign data.
 * @param d the dimensions of the data
 * @param N the number of the data
 * @param k the number of clusters
 * @param data input data
 * @param centers the centers
 * @param clusters the clusters
 * @param d_type the type of distance. Available options are NORM_L1, NORM_L2, HAMMING
 * @param n_thread the number of threads
 * @param verbose for debugging
 */
template<typename DataType>
void kd_nn_assign(
		DataType ** data,
		DataType ** centers,
		vector<i_vector>& clusters,
		DistanceType d_type,
		int d,
		int N,
		int k,
		int n_thread,
		bool verbose) {
	int i;
	int tmp;
	KDNode<DataType> * root = nullptr;
	make_random_tree<DataType>(root,centers,k,d,0,verbose);
	if(root == nullptr) return;

	KDNode<DataType> query(d);
#ifdef _OPENMP
	SET_THREAD_NUM;
#pragma omp parallel
	{
#pragma omp for private(i)
#endif
		for(i = 0; i < N; i++) {
			KDNode<DataType> * nn = nullptr;
			double min = FLT_MAX;
			int visited = 0;
			query.add_data(data[i]);
			// Find the minimum distances between d_tmp and a centroid
			nn_search<DataType>(root,&query,nn,d_type,min,d,0,visited,verbose);
			tmp = nn->id;
			// Assign the data[i] into cluster tmp
			clusters[tmp].push_back(static_cast<int>(i));
		}
#ifdef _OPENMP
	}
#endif
}

/**
 * After having a set of centers,
 * we need to assign data into each cluster respectively.
 * This solution uses ANN kd-tree search to assign data.
 * @param d the dimensions of the data
 * @param N the number of the data
 * @param k the number of clusters
 * @param data input data
 * @param centers the centers
 * @param clusters the clusters
 * @param d_type the type of distance. Available options are NORM_L1, NORM_L2, HAMMING
 * @param n_thread the number of threads
 * @param verbose for debugging
 */
template<typename DataType>
void kd_ann_assign(
		DataType ** data,
		DataType ** centers,
		vector<i_vector>& clusters,
		DistanceType d_type,
		int d,
		int N,
		int k,
		int n_thread,
		double alpha,
		bool verbose) {
	int i;
	int tmp;
	KDNode<DataType> * root = nullptr;
	make_random_tree<DataType>(root,centers,k,d,0,verbose);
	if(root == nullptr) return;

	KDNode<DataType> query(d);

#ifdef _OPENMP
	SET_THREAD_NUM;
#pragma omp parallel
	{
#pragma omp for private(i)
#endif
		for(i = 0; i < N; i++) {
			KDNode<DataType> * nn = nullptr;
			double min = FLT_MAX;
			int visited = 0;
			query.add_data(data[i]);
			// Find the minimum distances between d_tmp and a centroid
			ann_search<DataType>(root,&query,nn,d_type,min,alpha,d,0,visited,verbose);
			tmp = nn->id;
			// Assign the data[i] into cluster tmp
			clusters[tmp].push_back(static_cast<int>(i));
		}
#ifdef _OPENMP
	}
#endif
}


/**
 * Initialize for Greg's method
 * @param data the point data
 * @param centers the centers of clusters
 * @param sum the sum vector of all points in each cluster
 * @param upper upper bound on the distance between a point data and its assigned center
 * @param lower lower bound on the distance between a point data and its second closest center
 * @param label the label of point data that specify their assigned cluster
 * @param d_type the type of distance. Available options are NORM_L1, NORM_L2, HAMMING
 * @param N the size of data set
 * @param k the number of clusters
 * @param d the number of dimensions
 * @param n_thread the number of threads
 * @param verbose enable it to see the log
 * @return nothing
 */
template<typename DataType>
void greg_initialize(
		DataType ** data,
		float ** centers,
		float **& sum,
		float *& upper,
		float *& lower,
		int *& label,
		int *& size,
		DistanceType d_type,
		int N,
		int k,
		int d,
		int n_thread,
		bool verbose) {
	int i, j;
	// Initializing size and vector sum
	for(i = 0; i < k; i++) {
		size[i] = 0;
		for(j = 0; j < d; j++) {
			sum[i][j] = 0.0;
		}
	}

#ifdef _OPENMP
	SET_THREAD_NUM;
#pragma omp parallel
	{
#pragma omp for private(i,j)
#endif
		for(i = 0; i < N; i++) {
			float min = FLT_MAX;
			float min2 = FLT_MAX;
			float d_tmp = 0.0;
			int tmp = -1;
			for(j = 0; j < k; j++) {
				if(d_type == DistanceType::NORM_L2) {
					d_tmp = distance_l2<float,DataType>(centers[j],data[i],d);
				} else if(d_type == DistanceType::NORM_L1) {
					d_tmp = distance_l1<float,DataType>(centers[j],data[i],d);
				}
				if(min >= d_tmp) {
					min2 = min;
					min = d_tmp;
					tmp = j;
				} else {
					if(min2 >= d_tmp) min2 = d_tmp;
				}
			}

			label[i] = tmp; // Update the label
			upper[i] = min; // Update the upper bound on this distance
			lower[i] = min2; // Update the lower bound on this distance

			// Update the size
			size[tmp]++;

			// Update the vector sum
			for(j = 0; j < d; j++) {
				sum[tmp][j] += static_cast<float>(data[i][j]);
			}
		}
#ifdef _OPENMP
	}
#endif
}

/**
 * Update the centers
 * @param sum the sum vector of all points in the cluster
 * @param size the size of each cluster
 * @param centers the centers of clusters
 * @param moved the distances that centers moved
 * @param d_type the type of distance. Available options are NORM_L1, NORM_L2, HAMMING
 * @param k the number of clusters
 * @param d the number of dimensions
 * @param n_thread the number of threads
 * @return nothing
 */
void update_center(
		float ** sum,
		int * size,
		float **& centers,
		float *& moved,
		DistanceType d_type,
		int k,
		int d,
		int n_thread);

/**
 * Update the bounds
 * @param moved the distances that centers moved
 * @param label the labels of point data
 * @param upper
 * @param lower
 * @param N
 * @param k
 * @param d
 * @param n_thread the number of threads
 */
void update_bounds(
		float * moved,
		int * label,
		float *& upper,
		float *& lower,
		int N,
		int k,
		int n_thread);

/**
 * Calculate the distortion of a set of clusters.
 * @param d the dimensions of the data
 * @param N the number of the data
 * @param k the number of clusters
 * @param data input data
 * @param centers the centers
 * @param clusters the clusters
 * @param d_type the type of distance. Available options are NORM_L1, NORM_L2, HAMMING
 * @param n_thread the number of threads
 * @param verbose for debugging
 */
template<typename DataType>
float distortion(
		DataType ** data,
		float ** centers,
		int * label,
		DistanceType d_type,
		int d,
		int N,
		int k,
		int n_thread,
		bool verbose) {
	float e = 0.0;
	int i, j, start, end;
#ifdef _OPENMP
	SET_THREAD_NUM;
#pragma omp parallel
	{
#pragma omp for private(i,j,start,end)
#endif
		for(i = 0; i < n_thread; i++) {
			start = N / n_thread * i;
			end = N > start + N / n_thread ? (start + N / n_thread) : N;
			for(j = start; j < end; j++) {
				if(d_type == DistanceType::NORM_L2)
					e += distance_l2_square<DataType,float>(data[j],centers[label[j]],d);
				else if(d_type == DistanceType::NORM_L1)
					e += distance_l1<DataType,float>(data[j],centers[label[j]],d);
			}
		}
#ifdef _OPENMP
	}
#endif
	return sqrt(e);
}

/**
 * The k-means method: a description of the method can be found at
 * http://home.deib.polimi.it/matteucc/Clustering/tutorial_html/kmeans.html
 * @param type the type of seeding method
 * @param assign the type of assigning method
 * @param d the dimensions of the data
 * @param N the number of the data
 * @param k the number of clusters
 * @param criteria the criteria
 * @param data input data
 * @param centers the centers
 * @param label the labels of data points
 * @param seeds the initial centers = the seeds
 * @param d_type the type of distance. Available options are NORM_L1, NORM_L2, HAMMING
 * @param n_thread the number of threads
 * @param verbose for debugging
 */
template<typename DataType>
void simple_k_means(
		DataType ** data,
		float **& centers,
		int *& label,
		float **& seeds,
		KmeansType type,
		KmeansAssignType assign,
		KmeansCriteria criteria,
		DistanceType d_type,
		int N,
		int k,
		int d,
		int n_thread,
		bool verbose) {
	// Pre-check conditions
	if (N < k) {
		if(verbose)
			cerr << "There will be some empty clusters!" << endl;
		exit(1);
	}

	if(seeds == nullptr) {
		init_array_2<float>(seeds,k,d);
	}

	// Seeding
	if (type == KmeansType::RANDOM_SEEDS) {
		random_seeds<DataType>(data,seeds,d,N,k,n_thread,verbose);
	} else if(type == KmeansType::KMEANS_PLUS_SEEDS) {
		kmeans_pp_seeds<DataType>(data,seeds,d_type,d,N,k,n_thread,verbose);
	}

	if(verbose)
		cout << "Finished seeding" << endl;

	// Criteria's setup
	int iters = criteria.iterations, it = 0, count = 0;
	float error = criteria.accuracy, e = error, e_prev;

	// Variables for Greg's method
	float ** c_sum;
	float * moved;
	float * closest;
	int * farthest;
	float * df;
	float * upper;
	float * lower;
	int * size;

	init_array_2<float>(c_sum,k,d);
	init_array<float>(moved,k);
	init_array<float>(df,k);
	init_array<float>(closest,k);
	init_array<float>(upper,N);
	init_array<float>(lower,N);
	init_array<int>(size,k);
	init_array<int>(farthest,k);

	int i, j;
	int tmp = 0, max_pos, tmp2;
	float min, min2, min_tmp = 0.0,
			d_tmp = 0.0, m;

	// Initialize the centers
	copy_array_2<float>(seeds,centers,k,d);
	greg_initialize<DataType>(data,centers,c_sum,upper,lower,
			label,size,d_type,N,k,d,n_thread,verbose);
	// Initialize the farthest observers
	for(i = 0; i < k; i++) {
		df[i] = DBL_MIN;
		farthest[i] = -1;
	}
#ifdef _OPENMP
	SET_THREAD_NUM;
#pragma omp parallel
	{
#pragma omp for private(i,j)
#endif
		for(i = 0; i < N; i++) {
			for(j = 0; j < k; j++) {
				if(d_type == DistanceType::NORM_L2)
					d_tmp = distance_l2<DataType,float>(data[i],centers[j],d);
				else if(d_type == DistanceType::NORM_L1)
					d_tmp = distance_l1<DataType,float>(data[i],centers[j],d);
				if(df[j] < d_tmp) {
					df[j] = d_tmp;
					farthest[j] = i;
				}
			}
		}
#ifdef _OPENMP
	}
#endif
	if(verbose)
		cout << "Finished initialization" << endl;

	while (1) {
		// Update the closest distances
		for(i = 0; i < k; i++) {
			min2 = min = FLT_MAX;
			for(j = 0; j < k; j++) {
				if(j != i) {
					if(d_type == DistanceType::NORM_L2)
						min_tmp = distance_l2<float>(centers[i],centers[j],d);
					else if(d_type == DistanceType::NORM_L1)
						min_tmp = distance_l1<float>(centers[i],centers[j],d);
					if(min > min_tmp) min = min_tmp;
				}
			}
			closest[i] = min;
		}

#ifdef _OPENMP
		SET_THREAD_NUM;
#pragma omp parallel
		{
#pragma omp for private(i,j,d_tmp,m,min,min2,tmp,max_pos, tmp2)
#endif
			for(i = 0; i < N; i++) {
				// Update m for bound test
				d_tmp = closest[label[i]]/2.0;
				m = std::max(d_tmp,lower[i]);
				// First bound test
				if(upper[i] > m) {
					// We need to tighten the upper bound
					if(d_type == DistanceType::NORM_L2)
						upper[i] = distance_l2<DataType,float>(data[i],centers[label[i]],d);
					else if(d_type == DistanceType::NORM_L1)
						upper[i] = distance_l1<DataType,float>(data[i],centers[label[i]],d);
					// Second bound test
					if(upper[i] > m) {
						int l = label[i];
						min2 = min = FLT_MAX;
						tmp = -1;
						// Assign the data to clusters
						for(j = 0; j < k; j++) {
							if(d_type == DistanceType::NORM_L2)
								d_tmp = distance_l2<float,DataType>(centers[j],data[i],d);
							else if(d_type == DistanceType::NORM_L1)
								d_tmp = distance_l1<float,DataType>(centers[j],data[i],d);
							if(min >= d_tmp) {
								min2 = min;
								min = d_tmp;
								tmp = j;
							} else {
								if(min2 > d_tmp) min2 = d_tmp;
							}
						}

						// Assign the data[i] into cluster tmp
						label[i] = tmp; // Update the label
						upper[i] = min; // Update the upper bound on this distance
						lower[i] = min2; // Update the lower bound on this distance

						if(l != tmp) {
							size[tmp]++;
							size[l]--;
							if(size[l] == 0) {
								if(verbose)
									cout << "An empty cluster was found!"
									" label = " << l << endl;
								// Create a new cluster based on the furthest observer
								max_pos = farthest[l];
								tmp2 = label[max_pos];
								for(j = 0; j < d; j++) {
									c_sum[l][j] += static_cast<float>(data[max_pos][j]);
									c_sum[tmp2][j] -= static_cast<float>(data[max_pos][j]);
								}
								size[tmp2]--;
								size[l]++;
							}
							for(j = 0; j < d; j++) {
								c_sum[tmp][j] += static_cast<float>(data[i][j]);
								c_sum[l][j] -= static_cast<float>(data[i][j]);
							}
						}
					}
				}
			}
#ifdef _OPENMP
		}
#endif

		// Move the centers
		update_center(c_sum,size,centers,moved,d_type,k,d,n_thread);

#ifdef _OPENMP
	SET_THREAD_NUM;
#pragma omp parallel
	{
#pragma omp for private(i,j)
#endif
		for(i = 0; i < N; i++) {
			for(j = 0; j < k; j++) {
				if(d_type == DistanceType::NORM_L2)
					d_tmp = distance_l2<DataType,float>(data[i],centers[j],d);
				else if(d_type == DistanceType::NORM_L1)
					d_tmp = distance_l1<DataType,float>(data[i],centers[j],d);
				if(df[j] < d_tmp) {
					df[j] = d_tmp;
					farthest[j] = i;
				}
			}
		}
#ifdef _OPENMP
	}
#endif

		// Update the bounds
		update_bounds(moved,label,upper,lower,N,k,n_thread);

		// Calculate the distortion
		e_prev = e;
		e = 0.0;
		for(i = 0; i < k; i++) {
			e += moved[i];
		}
		e = sqrt(e);
		count += (fabs(e-e_prev) < error? 1 : 0);
		if(verbose)
			cout << "Iterator " << it
			<< "-th with error = " << e
			<< " and distortion = " << distortion(data,centers,label,d_type,d,N,k,n_thread,false)
			<< endl;
		it++;
		if(it >= iters || e < error || count >= 10) break;
	}

	if(verbose)
		cout << "Finished clustering with error is " <<
		e << " after " << i << " iterations." << endl;
}
}

#endif /* K_MEANS_H_ */
