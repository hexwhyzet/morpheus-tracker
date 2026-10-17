#ifndef __ESM__
#define __ESM__

using namespace std;
using namespace cv;

//void esmTracking(const Mat &, Mat &, Mat &, const vector<Point2f> &);
void esmRefine(const Mat &templateImg, const Mat &image, Mat &H, int maxIter = 10, double minNorm = 1e-4f);
void esmRefinePyr(const Mat &templateImg, const Mat &image, Mat &H, int maxIterPerLevel = 10, double minNorm = 1e-4, int maxLevel = 3);
//void esmRefine(const Mat &templateImg, const Mat &image, Mat &Pose);

#endif
