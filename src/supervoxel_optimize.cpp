#include "supervoxel_optimize.h"

using namespace svr_optimize;

svrOptimize::svrOptimize() {

}

svrOptimize::~svrOptimize() {

}

// Gauss Newton Optimization
void svrOptimize::optimizeUsingGaussNewton(Eigen::Affine3d& resultantTransform, float& cost) {

	svr::PointCloudT::Ptr scan2 = opt_data.scan2;
	svr::SVMap* svMap = opt_data.svMap;
	Eigen::Affine3d last_transform = opt_data.t;

	double x,y,z,roll,pitch,yaw;

	svr_util::transform_get_translation_from_affine(last_transform, &x, &y, &z);
	svr_util::transform_get_rotation_from_affine(last_transform, &roll, &pitch, &yaw);

	int maxIteration = 20, iteration = 0;
	bool debug = true, converged = false;
	double tol = 1e-3, stepSize = 1.;

	svr::SVMap::iterator svMapItr;
	SData::ScanIndexVector::iterator pItr;
	SData::ScanIndexVectorPtr indexVector;
	Eigen::Affine3d iterationTransform;

	while (!converged || iteration < maxIteration) {

		iteration++;
		iterationTransform = Eigen::Affine3d::Identity();
		iterationTransform.translation() << x,y,z;
		iterationTransform.rotate (Eigen::AngleAxisd (roll, Eigen::Vector3d::UnitX()));
		iterationTransform.rotate (Eigen::AngleAxisd (pitch, Eigen::Vector3d::UnitY()));
		iterationTransform.rotate(Eigen::AngleAxisd (yaw, Eigen::Vector3d::UnitZ()));

		// transform Scan
		svr::PointCloudT::Ptr transformedScan =  boost::shared_ptr<svr::PointCloudT>(new svr::PointCloudT());
		pcl::transformPointCloud(*scan2, *transformedScan, iterationTransform);

		// Calculate Hessian and Gradient at last pose

		Eigen::MatrixXf H(6,6);
		H.setZero();
		Eigen::VectorXf g(6,1);
		g.setZero();
		double cost = 0;

		// this iteration takes time
		for (svMapItr = svMap->begin(); svMapItr != svMap->end(); ++svMapItr) {

			SData::Ptr supervoxel = svMapItr->second;

			double d1 = supervoxel->getD1();
			double d2 = supervoxel->getD2();
			Eigen::Matrix3f covarianceInv = supervoxel->getCovarianceInverse();
			Eigen::Matrix3f covariance = supervoxel->getCovariance();
			Eigen::Vector4f mean = supervoxel->getCentroid();
			Eigen::Vector3f U;
			U << mean(0), mean(1), mean(2);

			indexVector = supervoxel->getScanBIndexVector();

			for (pItr = indexVector->begin(); pItr != indexVector->end(); ++pItr) {
				// calculate hessian, gradient contribution of individual points
				svr::PointT p = transformedScan->at(*pItr);
				Eigen::Vector3f X;
				X << p.x, p.y, p.z;

				// Using small angle approximation
				Eigen::MatrixXf Jacobian (3,6);
				Jacobian << 1,0,0,0,X(2),-X(1),
								0,1,0,-X(2),0,X(0),
								0,0,1,X(1),-X(0),0;

				float power = (X-U).transpose() * covarianceInv * (X-U);
				power = -d2 * power / 2;
				double exponentPower = exp(power);

				cost += d1 * exponentPower;
				//g = g + (-d1 * d2 * exponentPower * (X-U).transpose() * covarianceInv * Jacobian);
				g = g + (-d1 * d2 * exponentPower * Jacobian.transpose() * covarianceInv * (X-U));

				for (int i = 0; i < 6; i++) {
					for (int j = i; j < 6; j++) {
						double r = -d1*d2*exponentPower;
						double r2 = -d2 * ((X-U).transpose()*covarianceInv*Jacobian.col(i))*((X-U).transpose()*covarianceInv*Jacobian.col(j));
						double r3 = Jacobian.col(j).transpose()*covarianceInv*Jacobian.col(i);
						r = r * (r2+r3);
						H(i,j) = H(i,j) + r;
					}
				}

			}
		}

		for (int i = 0; i < 6; i++) {
			for (int j = i; j < 6; j++) {
				H(j,i) = H(i,j);
			}
		}

		// calculate pose step

		Eigen::VectorXf poseStep(6,1);
		poseStep = H.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(g);

		std::cout << "Iteration: " << iteration << std::endl;
		std::cout << "Cost: " << cost << std::endl;
		std::cout << "Hessian: " << std::endl << H << std::endl;
		std::cout << "Gradient: " << std::endl << g << std::endl;
		std::cout << "Pose Step: " << std::endl << poseStep << std::endl;

		x -= poseStep(0);
		y -= poseStep(1);
		z -= poseStep(2);
		roll -= poseStep(3);
		pitch -= poseStep(4);
		yaw -= poseStep(5);
	}
}


















