/*
 * EXOTicaPlannerService.cpp
 *
 *  Created on: 19 Mar 2015
 *      Author: yiming
 */

#include "exotica_moveit/EXOTicaPlannerService.h"

namespace exotica
{
	EXOTicaPlannerService::EXOTicaPlannerService() :
					as_(nh_, "/ExoticaPlanning", boost::bind(&exotica::EXOTicaPlannerService::solve, this, _1), false),
					initialised_(false)
	{

	}

	EXOTicaPlannerService::~EXOTicaPlannerService()
	{

	}

	bool EXOTicaPlannerService::initialise(const std::string & config, const std::string & solver,
			const std::string & problem, const std::string & group)
	{
		exotica::Initialiser ini;

		std::vector<exotica::MotionSolver_ptr> solvers;
		std::vector<exotica::PlanningProblem_ptr> probs;
		std::vector<std::string> solver_names(1), prob_names(2);
		solver_names[0] = solver;
		prob_names[0] = problem;
		prob_names[1] = problem + "Bias";
		INFO("Loading from "<<config);
		initialised_ = true;
		if (!ok(ini.initialise(config, server_, solvers, probs, prob_names, solver_names)))
		{
			ERROR("EXOTica/MoveIt Action service: EXOTica initialisation failed !!!!");
			initialised_ = false;
		}
		else
		{
			solver_ = solvers[0];
			problem_ = probs[0];
			if (solver_->type().compare("exotica::OMPLsolver") == 0)
			{
				if (problem_->getTaskMaps().find("CSpaceGoalMap") == problem_->getTaskMaps().end()
						|| problem_->getTaskMaps().at("CSpaceGoalMap")->type().compare("exotica::Identity")
								!= 0)
				{
					INDICATE_FAILURE
					initialised_ = false;
				}
				else
				{
					goal_map_ =
							boost::static_pointer_cast<exotica::Identity>(problem_->getTaskMaps().at("CSpaceGoalMap"));
				}

				if (probs[1]->getTaskMaps().find("GoalBiasMap") == probs[1]->getTaskMaps().end()
						|| probs[1]->getTaskMaps().at("GoalBiasMap")->type().compare("exotica::Identity")
								!= 0)
				{
					INDICATE_FAILURE
					initialised_ = false;
				}
				else
				{
					goal_bias_map_ =
							boost::static_pointer_cast<exotica::Identity>(probs[1]->getTaskMaps().at("GoalBiasMap"));
				}

				const moveit::core::JointModelGroup* model_group =
						server_->getModel("robot_description")->getJointModelGroup(group);
				moveit::core::JointBoundsVector b = model_group->getActiveJointModelsBounds();
				exotica::OMPLProblem_ptr tmp =
						boost::static_pointer_cast<exotica::OMPLProblem>(problem_);
				tmp->getBounds().resize(b.size() * 2);
				for (int i = 0; i < b.size(); i++)
				{
					tmp->getBounds()[i] = (*b[i])[0].min_position_;
					tmp->getBounds()[i + b.size()] = (*b[i])[0].max_position_;
				}
				if (!exotica::ok(boost::static_pointer_cast<exotica::OMPLsolver>(solver_)->specifyProblem(probs[0], NULL, probs[1], NULL)))
				{
					INDICATE_FAILURE
					initialised_ = false;
				}
			}
			else if (!exotica::ok(solver_->specifyProblem(problem_)))
			{
				INDICATE_FAILURE
				initialised_ = false;
			}

			if (initialised_)
				as_.start();
		}
		return initialised_;
	}
	bool EXOTicaPlannerService::solve(const exotica_moveit::ExoticaPlanningGoalConstPtr & goal)
	{
		res_.succeeded_ = false;
		scene_.reset(new moveit_msgs::PlanningScene(goal->scene_));
		if (!ok(problem_->setScene(scene_)))
		{
			INDICATE_FAILURE
			return false;
		}
//		HIGHLIGHT_NAMED("MoveitInterface", "Using Solver "<<solver_->object_name_<<"["<<solver_->type()<<"], Problem "<<problem_->object_name_<<"["<<problem_->type()<<"].");

		for (int i = 0; i < 50; i++)
		{
			if (solver_->type().compare("exotica::OMPLsolver") == 0)
			{
				exotica::OMPLsolver_ptr ss =
						boost::static_pointer_cast<exotica::OMPLsolver>(solver_);

				Eigen::VectorXd qT;
				exotica::vectorExoticaToEigen(goal->qT, qT);
				goal_bias_map_->jointRef = qT;
				goal_map_->jointRef = qT;
				ss->setGoalState(qT, 1e-4);
				ss->setMaxPlanningTime(goal->max_time_);
				if (!ok(ss->resetIfNeeded()))
				{
					INDICATE_FAILURE
					return FAILURE;
				}
			}

			EReturn found = FAILURE;
			Eigen::VectorXd q0;
			Eigen::MatrixXd solution;
			exotica::vectorExoticaToEigen(goal->q0, q0);
			ros::Time start = ros::Time::now();
			if (solver_->type().compare("exotica::IKsolver") == 0)
			{
				found =
						boost::static_pointer_cast<exotica::IKsolver>(solver_)->SolveFullSolution(q0, solution);
			}
			else
				found = solver_->Solve(q0, solution);
			if (ok(found))
			{
				res_.succeeded_ = true;
				fb_.solving_time_ = res_.planning_time_ =
						ros::Duration(ros::Time::now() - start).toSec();
				exotica::matrixEigenToExotica(solution, res_.solution_);
				as_.setSucceeded(res_);
			}
			HIGHLIGHT_NAMED("OMPL Benchmark", "Solving time "<<i<<"/50 "<< (ok(found) ? "Succeed":"Failed"));
		}
		return res_.succeeded_;
	}
}
