#include "stdafx.h"
#include <opencv2\xfeatures2d\nonfree.hpp>
#include <opencv2\features2d\features2d.hpp>
#include <opencv2\highgui\highgui.hpp>
#include <opencv2\calib3d\calib3d.hpp>
#include <opencv2\opencv.hpp>
#include <iostream>
#include <tinydir.h>
//#include "MouseRespone.h"

using namespace cv;
using namespace std;

//��ȡͼ��SIFT����
void extract_features(vector<string>& image_names,
	vector<vector<KeyPoint>>& key_points_for_all,
	vector<Mat>& descriptor_for_all,
	vector<vector<Vec3b>>& colors_for_all) {

	key_points_for_all.clear();
	descriptor_for_all.clear();
	Mat image;

	//��ȡͼ�񣬻�ȡͼ�������㣬������
	Ptr<Feature2D> sift = xfeatures2d::SIFT::create(0, 3, 0.04, 10);
	for (auto it = image_names.begin(); it != image_names.end(); ++it) {

		image = imread(*it);
		if (image.empty()) continue;

		cout << "Extracing features: " << *it << endl;

		vector<KeyPoint> key_points;
		Mat descriptor;

		//ż�������ڴ����ʧ�ܵĴ���,
		//���Կ�����main()֮ǰʹ��initModule_nonfree();��ʼ��ģ��
		sift->detectAndCompute(image, noArray(), key_points, descriptor);

		//��������٣����ų���ͼ��
		if (key_points.size() <= 10) continue;

		key_points_for_all.push_back(key_points);
		descriptor_for_all.push_back(descriptor);

		vector<Vec3b> colors(key_points.size());
		for (int i = 0; i < key_points.size(); ++i) {

			Point2f& p = key_points[i].pt;
			//colors[i] = 255;  //��ɫֵ��
			colors[i] = image.at<Vec3b>(p.y, p.x);
		}
		colors_for_all.push_back(colors);
	}
}

//����KNN��ƥ��
void match_features(Mat& query, Mat& train, vector<DMatch>& matches) {

	vector<vector<DMatch>> knn_matches;
	BFMatcher matcher(NORM_L2);
	matcher.knnMatch(query, train, knn_matches, 2);

	//��ȡ����Ratio Test����Сƥ��ľ���
	float min_dist = FLT_MAX;
	for (int r = 0; r < knn_matches.size(); ++r) {

		//Ratio Test
		if (knn_matches[r][0].distance > 0.6*knn_matches[r][1].distance)
			continue;

		float dist = knn_matches[r][0].distance;
		if (dist < min_dist) min_dist = dist;
	}

	matches.clear();
	for (size_t r = 0; r < knn_matches.size(); ++r) {

		//�ų�������Ratio Test�ĵ��ƥ��������ĵ�
		if (knn_matches[r][0].distance > 0.6*knn_matches[r][1].distance ||
			knn_matches[r][0].distance > 5 * max(min_dist, 10.0f))
			continue;

		//����ƥ���
		matches.push_back(knn_matches[r][0]);
	}
	//Ҳ����ʹ��Cross Test��������֤���������ų�����
}

//ƥ�������ʾ
void match_features(vector<Mat>& descriptor_for_all, vector<vector<DMatch>>& matches_for_all) {

	matches_for_all.clear();
	// n��ͼ������˳���� n-1 ��ƥ��
	// 1��2ƥ�䣬2��3ƥ�䣬3��4ƥ�䣬�Դ�����
	for (int i = 0; i < descriptor_for_all.size() - 1; ++i) {

		cout << "Matching images " << i << " - " << i + 1 << endl;
		vector<DMatch> matches;
		match_features(descriptor_for_all[i], descriptor_for_all[i + 1], matches);
		matches_for_all.push_back(matches);
	}
}

//����ֵ�ֽ�
bool find_transform(Mat& K, vector<Point2f>& p1, vector<Point2f>& p2, Mat& R, Mat& T, Mat& mask) {

	//�����ڲξ����ȡ����Ľ���͹������꣨�������꣩
	double focal_length = 0.5*(K.at<double>(0) + K.at<double>(4));
	Point2d principle_point(K.at<double>(2), K.at<double>(5));

	//����ƥ�����ȡ��������ʹ��RANSAC����һ���ų�ʧ���
	Mat E = findEssentialMat(p1, p2, focal_length, principle_point, RANSAC, 0.999, 1.0, mask);
	if (E.empty()) return false;

	double feasible_count = countNonZero(mask);
	cout << (int)feasible_count << " -in- " << p1.size() << endl;
	//����RANSAC���ԣ�outlier��������50%ʱ������ǲ��ɿ���
	if (feasible_count <= 15 || (feasible_count / p1.size()) < 0.6)
		return false;

	//�ֽⱾ�����󣬻�ȡ��Ա任
	int pass_count = recoverPose(E, p1, p2, R, T, focal_length, principle_point, mask);

	//ͬʱλ���������ǰ���ĵ������Ҫ�㹻��
	if (((double)pass_count) / feasible_count < 0.7)
		return false;

	return true;
}

//��ƥ��
void get_matched_points(vector<KeyPoint>& p1,
	vector<KeyPoint>& p2,
	vector<DMatch> matches,
	vector<Point2f>& out_p1,
	vector<Point2f>& out_p2) {

	out_p1.clear();
	out_p2.clear();
	for (int i = 0; i < matches.size(); ++i) {

		out_p1.push_back(p1[matches[i].queryIdx].pt);
		out_p2.push_back(p2[matches[i].trainIdx].pt);
	}
}

//��ɫƥ��
void get_matched_colors(vector<Vec3b>& c1,
	vector<Vec3b>& c2,
	vector<DMatch> matches,
	vector<Vec3b>& out_c1,
	vector<Vec3b>& out_c2) {

	out_c1.clear();
	out_c2.clear();
	for (int i = 0; i < matches.size(); ++i) {

		out_c1.push_back(c1[matches[i].queryIdx]);
		out_c2.push_back(c2[matches[i].trainIdx]);
	}
}

//���ǻ��ؽ�
void reconstruct(Mat& K, Mat& R1, Mat& T1, Mat& R2, Mat& T2, vector<Point2f>& p1, vector<Point2f>& p2, vector<Point3f>& structure) {

	//���������ͶӰ����[R T]��triangulatePointsֻ֧��float��
	Mat proj1(3, 4, CV_32FC1);
	Mat proj2(3, 4, CV_32FC1);

	R1.convertTo(proj1(Range(0, 3), Range(0, 3)), CV_32FC1);
	T1.convertTo(proj1.col(3), CV_32FC1);

	R2.convertTo(proj2(Range(0, 3), Range(0, 3)), CV_32FC1);
	T2.convertTo(proj2.col(3), CV_32FC1);

	Mat fK;
	K.convertTo(fK, CV_32FC1);
	proj1 = fK*proj1;
	proj2 = fK*proj2;

	//�����ؽ�
	Mat s;
	triangulatePoints(proj1, proj2, p1, p2, s);

	structure.clear();
	structure.reserve(s.cols);
	for (int i = 0; i < s.cols; ++i) {

		Mat_<float> col = s.col(i);
		col /= col(3);	//������꣬��Ҫ�������һ��Ԫ�ز�������������ֵ
		structure.push_back(Point3f(col(0), col(1), col(2)));
	}
}

void maskout_points(vector<Point2f>& p1, Mat& mask) {

	vector<Point2f> p1_copy = p1;
	p1.clear();

	for (int i = 0; i < mask.rows; ++i) {

		if (mask.at<uchar>(i) > 0)
			p1.push_back(p1_copy[i]);
	}
}

void maskout_colors(vector<Vec3b>& p1, Mat& mask) {

	vector<Vec3b> p1_copy = p1;
	p1.clear();

	for (int i = 0; i < mask.rows; ++i) {

		if (mask.at<uchar>(i) > 0)
			p1.push_back(p1_copy[i]);
	}
}

void save_structure(string file_name, vector<Mat>& rotations, vector<Mat>& motions, vector<Point3f>& structure, vector<Vec3b>& colors) {

	int n = (int)rotations.size();

	FileStorage fs(file_name, FileStorage::WRITE);
	fs << "Camera Count" << n;
	fs << "Point Count" << (int)structure.size();

	fs << "Rotations" << "[";
	for (size_t i = 0; i < n; ++i) {

		fs << rotations[i];
	}
	fs << "]";

	fs << "Motions" << "[";
	for (size_t i = 0; i < n; ++i) {

		fs << motions[i];
	}
	fs << "]";

	fs << "Points" << "[";
	for (size_t i = 0; i < structure.size(); ++i) {

		fs << structure[i];
	}
	fs << "]";

	fs << "Colors" << "[";
	for (size_t i = 0; i < colors.size(); ++i) {

		fs << colors[i];
	}
	fs << "]";

	fs.release();
}

void get_objpoints_and_imgpoints(vector<DMatch>& matches,
	vector<int>& struct_indices,
	vector<Point3f>& structure,
	vector<KeyPoint>& key_points,
	vector<Point3f>& object_points,
	vector<Point2f>& image_points) {

	object_points.clear();
	image_points.clear();

	for (int i = 0; i < matches.size(); ++i) {

		int query_idx = matches[i].queryIdx;
		int train_idx = matches[i].trainIdx;

		int struct_idx = struct_indices[query_idx];
		if (struct_idx < 0) continue;

		object_points.push_back(structure[struct_idx]);
		image_points.push_back(key_points[train_idx].pt);
	}
}

void fusion_structure(vector<DMatch>& matches,
	vector<int>& struct_indices,
	vector<int>& next_struct_indices,
	vector<Point3f>& structure,
	vector<Point3f>& next_structure,
	vector<Vec3b>& colors,
	vector<Vec3b>& next_colors) {

	for (int i = 0; i < matches.size(); ++i) {

		int query_idx = matches[i].queryIdx;
		int train_idx = matches[i].trainIdx;

		int struct_idx = struct_indices[query_idx];
		if (struct_idx >= 0) {//���õ��ڿռ����Ѿ����ڣ������ƥ����Ӧ�Ŀռ��Ӧ����ͬһ��������Ҫ��ͬ

			next_struct_indices[train_idx] = struct_idx;
			continue;
		}

		//���õ��ڿռ����Ѿ����ڣ����õ���뵽�ṹ�У������ƥ���Ŀռ��������Ϊ�¼���ĵ������
		structure.push_back(next_structure[i]);
		colors.push_back(next_colors[i]);
		struct_indices[query_idx] = next_struct_indices[train_idx] = structure.size() - 1;
	}
}

void init_structure(Mat K,
	vector<vector<KeyPoint>>& key_points_for_all,
	vector<vector<Vec3b>>& colors_for_all,
	vector<vector<DMatch>>& matches_for_all,
	vector<Point3f>& structure,
	vector<vector<int>>& correspond_struct_idx,
	vector<Vec3b>& colors,
	vector<Mat>& rotations,
	vector<Mat>& motions,
	vector<Point2f>& p1) {

	//����ͷ����ͼ��֮��ı任����
	vector<Point2f> p2;
	vector<Vec3b> c2;
	Mat R, T;	//��ת�����ƽ������
	Mat mask;	//mask�д�����ĵ����ƥ��㣬���������ʧ���
	get_matched_points(key_points_for_all[0], key_points_for_all[1], matches_for_all[0], p1, p2);
	get_matched_colors(colors_for_all[0], colors_for_all[1], matches_for_all[0], colors, c2);
	find_transform(K, p1, p2, R, T, mask);

	//��ͷ����ͼ�������ά�ؽ�
	maskout_points(p1, mask);
	maskout_points(p2, mask);
	maskout_colors(colors, mask);

	Mat R0 = Mat::eye(3, 3, CV_64FC1);
	Mat T0 = Mat::zeros(3, 1, CV_64FC1);
	reconstruct(K, R0, T0, R, T, p1, p2, structure);

	//����任����
	rotations = { R0, R };
	motions = { T0, T };

	//��correspond_struct_idx�Ĵ�С��ʼ��Ϊ��key_points_for_all��ȫһ��
	correspond_struct_idx.clear();
	correspond_struct_idx.resize(key_points_for_all.size());
	for (int i = 0; i < key_points_for_all.size(); ++i) {

		correspond_struct_idx[i].resize(key_points_for_all[i].size(), -1);
	}

	//��дͷ����ͼ��Ľṹ����
	int idx = 0;
	vector<DMatch>& matches = matches_for_all[0];
	for (int i = 0; i < matches.size(); ++i) {

		if (mask.at<uchar>(i) == 0)
			continue;

		correspond_struct_idx[0][matches[i].queryIdx] = idx;
		correspond_struct_idx[1][matches[i].trainIdx] = idx;
		++idx;
	}
}


void get_file_names(string dir_name, vector<string> & names) {

	names.clear();
	tinydir_dir dir;
	tinydir_open(&dir, dir_name.c_str());

	while (dir.has_next) {

		tinydir_file file;
		tinydir_readfile(&dir, &file);
		if (!file.is_dir) {

			names.push_back(file.path);
		}
		tinydir_next(&dir);
	}
	tinydir_close(&dir);
}

//��ǰȷ��ƽ������
Rect select;
bool select_flag = false;
Mat img, showImg;
Point dot_one, dot_two;
char temp[16];

void A_on_Mouse(int event, int x, int y, int flags, void*param) {  //ʵ�ֻ����ο�

	Point p1, p2;
	if (event == EVENT_LBUTTONDOWN) {

		select.x = x;
		select.y = y;
		select_flag = true;
	}
	else if (select_flag &&event == EVENT_MOUSEMOVE) {

		img.copyTo(showImg);
		p1 = Point(select.x, select.y);
		p2 = Point(x, y);
		rectangle(showImg, p1, p2, Scalar(0, 255, 0), 2);
		dot_one = p1; dot_two = p2;  //�������¼

		sprintf(temp, "(%d, %d)", x, y);  //����ʵʱ��ʾ
		putText(showImg, temp, p2, FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 0, 255));
		imshow("img", showImg);
	}
	else if (select_flag && event == EVENT_LBUTTONUP) {

		select_flag = false;
	}
}


void B_on_Mouse(int event, int x, int y, int flags, void*param) {//ʵ�ֻ����ο򲢽�ͼ  

	Point p1, p2;
	switch (event) {

	case  EVENT_LBUTTONDOWN: {

		select.x = x;
		select.y = y;
		select_flag = true;
	}
							 break;
	case EVENT_MOUSEMOVE: {

		if (select_flag) {

			img.copyTo(showImg);
			p1 = Point(select.x, select.y);
			p2 = Point(x, y);
			rectangle(showImg, p1, p2, Scalar(0, 255, 0), 2);
			dot_one = p1; dot_two = p2;

			sprintf(temp, "(%d, %d)", x, y);  //����ʵʱ��ʾ
			putText(showImg, temp, p2, FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 0, 255));
			imshow("img", showImg);
		}
	}
						  break;
	case EVENT_LBUTTONUP: {

		//��ʾ�����ROI  
		Rect roi = Rect(Point(select.x, select.y), Point(x, y));
		if (roi.width && roi.height) {//��һ��ʱ��û�з�Ӧ

			Mat roiImg = img(roi);
			imshow("roi", roiImg);
		}
		select_flag = false;
	}

						  break;
	}
}

//Ax+by+cz=D  ���ƽ�溯��
void cvFitPlane(const CvMat* points, float* plane) {
	// Estimate geometric centroid.  
	int nrows = points->rows;
	int ncols = points->cols;
	int type = points->type;
	CvMat* centroid = cvCreateMat(1, ncols, type);
	cvSet(centroid, cvScalar(0));
	for (int c = 0; c<ncols; c++) {
		for (int r = 0; r < nrows; r++)
		{
			centroid->data.fl[c] += points->data.fl[ncols*r + c];
		}
		centroid->data.fl[c] /= nrows;
	}
	// Subtract geometric centroid from each point.  
	CvMat* points2 = cvCreateMat(nrows, ncols, type);
	for (int r = 0; r<nrows; r++)
		for (int c = 0; c<ncols; c++)
			points2->data.fl[ncols*r + c] = points->data.fl[ncols*r + c] - centroid->data.fl[c];
	// Evaluate SVD of covariance matrix.  
	CvMat* A = cvCreateMat(ncols, ncols, type);
	CvMat* W = cvCreateMat(ncols, ncols, type);
	CvMat* V = cvCreateMat(ncols, ncols, type);
	cvGEMM(points2, points, 1, NULL, 0, A, CV_GEMM_A_T);
	cvSVD(A, W, NULL, V, CV_SVD_V_T);
	// Assign plane coefficients by singular vector corresponding to smallest singular value.  
	plane[ncols] = 0;
	for (int c = 0; c<ncols; c++) {
		plane[c] = V->data.fl[ncols*(ncols - 1) + c];
		plane[ncols] += plane[c] * centroid->data.fl[c];
	}
	// Release allocated resources.  
	cvReleaseMat(&centroid);
	cvReleaseMat(&points2);
	cvReleaseMat(&A);
	cvReleaseMat(&W);
	cvReleaseMat(&V);
}

//����ѡ������Ŀռ�ƽ��
void DeterPlan(Point gain_one, Point gain_two, vector<Point3f>& struc, vector<Point2f>& repoint, float *plane) {

	//ɸѡѡȡ��
	vector<Point3f> select_pont;
	for (size_t t = 0; t < repoint.size(); t++) {
		Point pro = repoint[t];
		if ((pro.x >= gain_one.x * 5 && pro.x <= gain_two.x * 5) && (pro.y >= gain_one.y * 5 && pro.y <= gain_two.y * 5)) {

			select_pont.push_back(struc[t]);
		}
	}

	//���ƽ��
	CvMat*points_mat = cvCreateMat(select_pont.size(), 3, CV_32FC1);  //���������洢��Ҫ��ϵ�ľ���   
	for (int i = 0; i < select_pont.size(); ++i) {

		points_mat->data.fl[i * 3 + 0] = select_pont[i].x;  //�����ֵ���г�ʼ��   X������ֵ  
		points_mat->data.fl[i * 3 + 1] = select_pont[i].y;  //Y������ֵ  
		points_mat->data.fl[i * 3 + 2] = select_pont[i].z;  //������ֵ</span>  
															//<span style = "font-family: Arial, Helvetica, sans-serif;">
	}

	//plane[4] = { 0 };//������������ƽ�����������   

	cvFitPlane(points_mat, plane);//���÷���   
}


//������ɫ����
void AdjustColor(float *plane,
	vector<Point3f>& structure,
	vector<Vec3b>& colors,
	vector<Vec3b>& new_color) {

	for (size_t t = 0; t < structure.size(); t++) {

		double distance = fabs(double(plane[0] * structure[t].x + plane[1] * structure[t].y + plane[2] * structure[t].z - plane[3])) /
			sqrt(double(pow(double(plane[0]), 2) + pow(double(plane[1]), 2) + pow(double(plane[2]), 2)));
		if (distance < 0.1) {

			new_color.push_back({ 0, 255, 0 });
		}
		else if (distance < 0.8) {

			new_color.push_back({ 255, 0, 0 });
		}
		else {

			new_color.push_back({ 0, 0, 255 });
		}
	}
}


void main() {

	//ǰ��������ж�
	Mat img_pro = imread("./data_pro\\02.jpg", 1);

	double scale = 0.2;  //����
	Size dsize = Size(img_pro.cols*scale, img_pro.rows*scale);
	resize(img_pro, img, dsize);

	showImg = img.clone();
	select.x = select.y = 0;
	imshow("img", showImg);

	while (1) {

		int key = waitKey(10);
		switch (key) {

		case 'a':
			setMouseCallback("img", A_on_Mouse, 0);
			break;
		case 'b':
			setMouseCallback("img", B_on_Mouse, 0);
			break;
		}
		if (key == 27 || key == 'q')
			break;
	}
	waitKey(0);
	dot_one; dot_two;

	vector<string> img_names;
	get_file_names("data_pro", img_names);

	//��������

	double f = 5472 * 1.4 / 13.2;
	Mat K(Matx33d(
		f, 0, 5472 / 2,
		0, f, 3648 / 2,
		0, 0, 1));
	/*
	//�ṩ����ͼƬ�ı�������
	Mat K(Matx33d(
	2759.48, 0, 1520.69,
	0, 2764.16, 1006.81,
	0, 0, 1));*/

	vector<vector<KeyPoint>> key_points_for_all;
	vector<Mat> descriptor_for_all;
	vector<vector<Vec3b>> colors_for_all;
	vector<vector<DMatch>> matches_for_all;
	//��ȡ����ͼ�������
	extract_features(img_names, key_points_for_all, descriptor_for_all, colors_for_all);
	//������ͼ�����˳�ε�����ƥ��
	match_features(descriptor_for_all, matches_for_all);

	vector<Point3f> structure;
	vector<vector<int>> correspond_struct_idx; //�����i��ͼ���е�j���������Ӧ��structure�е������
	vector<Vec3b> colors_one;
	vector<Mat> rotations;
	vector<Mat> motions;
	vector<Point2f> standard_point;

	//��ʼ���ṹ����ά���ƣ�
	init_structure(K,
		key_points_for_all,
		colors_for_all,
		matches_for_all,
		structure,
		correspond_struct_idx,
		colors_one,
		rotations,
		motions,
		standard_point);

	//����ѡ������Ŀռ�ƽ��
	float plane12[4] = { 0 };//������������ƽ�����������
	DeterPlan(dot_one, dot_two, structure, standard_point, plane12);  //Distance:1.149533936707294

																	  //������ɫ����
	vector<Vec3b> colors;
	AdjustColor(plane12,
		structure,
		colors_one,
		colors);

	//������ʽ�ؽ�ʣ���ͼ��
	for (int i = 1; i < matches_for_all.size(); ++i) {

		vector<Point3f> object_points;
		vector<Point2f> image_points;
		Mat r, R, T;
		//Mat mask;

		//��ȡ��i��ͼ����ƥ����Ӧ����ά�㣬�Լ��ڵ�i+1��ͼ���ж�Ӧ�����ص�
		get_objpoints_and_imgpoints(matches_for_all[i],
			correspond_struct_idx[i],
			structure,
			key_points_for_all[i + 1],
			object_points,
			image_points);

		//���任����
		solvePnPRansac(object_points, image_points, K, noArray(), r, T);
		//����ת����ת��Ϊ��ת����
		Rodrigues(r, R);
		//����任����
		rotations.push_back(R);
		motions.push_back(T);

		vector<Point2f> p1, p2;
		vector<Vec3b> c1, c2;
		get_matched_points(key_points_for_all[i], key_points_for_all[i + 1], matches_for_all[i], p1, p2);
		get_matched_colors(colors_for_all[i], colors_for_all[i + 1], matches_for_all[i], c1, c2);

		//����֮ǰ��õ�R��T������ά�ؽ�
		vector<Point3f> next_structure;
		reconstruct(K, rotations[i], motions[i], R, T, p1, p2, next_structure);

		//������ɫ
		vector<Vec3b> color_other;
		AdjustColor(plane12,
			next_structure,
			c1,
			color_other);

		//���µ��ؽ������֮ǰ���ں�
		fusion_structure(matches_for_all[i],
			correspond_struct_idx[i],
			correspond_struct_idx[i + 1],
			structure,
			next_structure,
			colors,
			color_other);
	}

	//����
	save_structure(".\\Viewer\\structure.yml", rotations, motions, structure, colors);
}