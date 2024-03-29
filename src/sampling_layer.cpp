/*********************************************************************
 *
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2008, 2013, Willow Garage, Inc.
 *  Copyright (c) 2020, Samsung R&D Institute Russia
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Eitan Marder-Eppstein
 *         David V. Lu!!
 *         Alexey Merzlyakov
 *
 * Reference tutorial:
 * https://navigation.ros.org/tutorials/docs/writing_new_costmap2d_plugin.html
 *********************************************************************/
#include "nav2_sampling_costmap_plugin/sampling_layer.hpp"

#include "nav2_costmap_2d/costmap_math.hpp"
#include "nav2_costmap_2d/footprint.hpp"
#include "nav2_costmap_2d/costmap_2d.hpp"

#include "rclcpp/parameter_events_filter.hpp"

using nav2_costmap_2d::LETHAL_OBSTACLE;
using nav2_costmap_2d::INSCRIBED_INFLATED_OBSTACLE;
using nav2_costmap_2d::NO_INFORMATION;
using nav2_costmap_2d::FREE_SPACE;
using nav2_costmap_2d::MAX_NON_OBSTACLE;
using namespace nav2_costmap_2d;

namespace nav2_costmap_2d {
  static constexpr unsigned char NOT_IN_GRAPH_SPACE = 25;
}

using nav2_costmap_2d::NOT_IN_GRAPH_SPACE;

namespace nav2_sampling_costmap_plugin
{

SamplingLayer::SamplingLayer()
: last_min_x_(-std::numeric_limits<float>::max()),
  last_min_y_(-std::numeric_limits<float>::max()),
  last_max_x_(std::numeric_limits<float>::max()),
  last_max_y_(std::numeric_limits<float>::max())
{ 
}

// This method is called at the end of plugin initialization.
// It contains ROS parameter(s) declaration and initialization
// of need_recalculation_ variable.
void
SamplingLayer::onInitialize()
{
  auto node = node_.lock(); 
  declareParameter("enabled", rclcpp::ParameterValue(true));
  node->get_parameter(name_ + "." + "enabled", enabled_);
  declareParameter("dummy", rclcpp::ParameterValue(true));
  node->get_parameter(name_ + "." + "dummy", dummy_);
  //Get sample points from params file
  declareParameter("sample_points_x", rclcpp::ParameterValue(std::vector<long int>()));
  declareParameter("sample_points_y", rclcpp::ParameterValue(std::vector<long int>()));
  node->get_parameter(name_ + "." + "sample_points_x", sample_points_x_);
  node->get_parameter(name_ + "." + "sample_points_y", sample_points_y_);
  //Get connected sampling points from params file
  declareParameter("connections_1",rclcpp::ParameterValue(std::vector<long int>()));
  declareParameter("connections_2",rclcpp::ParameterValue(std::vector<long int>()));
  node->get_parameter(name_ + "." + "connections_1", connections_1_);
  node->get_parameter(name_ + "." + "connections_2", connections_2_);

  std::string samples = "";
  for(long unsigned i = 0 ; i < std::min(sample_points_x_.size(),sample_points_y_.size()) ; i++){
    samples += "(" + std::to_string(sample_points_x_[i]) + "," + std::to_string(sample_points_y_[i]) + "), ";
  }

  RCLCPP_INFO(
    logger_,
    "Puntos añadidos: %s",samples.c_str());

  //We get the points separated by blank spaces
  /*std::stringstream ss_x(x_samples),ss_y(y_samples);

  std::string x_str,y_str;

  while(ss_x >> x_str && ss_y >> y_str){
    double x_curr = std::stod(x_str), y_curr = std::stod(y_str);
    sample_points_x_.push_back(x_curr);
    sample_points_y_.push_back(y_curr);

    
  }*/

  need_recalculation_ = false;
  current_ = true;
}


void
SamplingLayer::updateSamplePoints(
  nav2_costmap_2d::Costmap2D & master_grid, int min_i, int min_j,
  int max_i,
  int max_j){

  unsigned char * master_array = master_grid.getCharMap();
  unsigned int size_x = master_grid.getSizeInCellsX(), size_y = master_grid.getSizeInCellsY();

  min_i = std::max(0, min_i);
  min_j = std::max(0, min_j);
  max_i = std::min(static_cast<int>(size_x), max_i);
  max_j = std::min(static_cast<int>(size_y), max_j);

  for(long unsigned i = 0 ; i < std::min(sample_points_x_.size(),sample_points_y_.size()) ; i++){
    int x = sample_points_x_[i],y = sample_points_y_[i];
    if( x > min_i && x < max_i && y > min_j && y < max_j){
      unsigned sampling_idx = master_grid.getIndex(x,y);
      master_array[sampling_idx] = 0;
      /*RCLCPP_INFO(
      logger_,
      "Punto válido y actualizado: (%d,%d)",x,y);*/
    }
    
  }
}

void
SamplingLayer::updateConnections(
  nav2_costmap_2d::Costmap2D & master_grid, int min_i, int min_j,
  int max_i,
  int max_j){

    unsigned char * master_array = master_grid.getCharMap();

    for(long unsigned i = 0 ; i < std::min(connections_1_.size(),connections_2_.size()) ; i++){
      unsigned x0 = sample_points_x_[connections_1_[i]], y0 = sample_points_y_[connections_1_[i]],
               x1 = sample_points_x_[connections_2_[i]], y1 = sample_points_y_[connections_2_[i]];
      MarkCell marker(master_array, FREE_SPACE);

      raytraceLine(marker, x0, y0, x1, y1);
    }
  }
// The method is called to ask the plugin: which area of costmap it needs to update.
// Inside this method window bounds are re-calculated if need_recalculation_ is true
// and updated independently on its value.
void
SamplingLayer::updateBounds(
  double /*robot_x*/, double /*robot_y*/, double /*robot_yaw*/, double * min_x,
  double * min_y, double * max_x, double * max_y)
{
  if (need_recalculation_) {
    last_min_x_ = *min_x;
    last_min_y_ = *min_y;
    last_max_x_ = *max_x;
    last_max_y_ = *max_y;
    // For some reason when I make these -<double>::max() it does not
    // work with Costmap2D::worldToMapEnforceBounds(), so I'm using
    // -<float>::max() instead.
    *min_x = -std::numeric_limits<float>::max();
    *min_y = -std::numeric_limits<float>::max();
    *max_x = std::numeric_limits<float>::max();
    *max_y = std::numeric_limits<float>::max();
    need_recalculation_ = false;
  } else {
    double tmp_min_x = last_min_x_;
    double tmp_min_y = last_min_y_;
    double tmp_max_x = last_max_x_;
    double tmp_max_y = last_max_y_;
    last_min_x_ = *min_x;
    last_min_y_ = *min_y;
    last_max_x_ = *max_x;
    last_max_y_ = *max_y;
    *min_x = std::min(tmp_min_x, *min_x);
    *min_y = std::min(tmp_min_y, *min_y);
    *max_x = std::max(tmp_max_x, *max_x);
    *max_y = std::max(tmp_max_y, *max_y);
  }
}

// The method is called when footprint was changed.
// Here it just resets need_recalculation_ variable.
void
SamplingLayer::onFootprintChanged()
{
  need_recalculation_ = true;

  RCLCPP_DEBUG(rclcpp::get_logger(
      "nav2_costmap_2d"), "SamplingLayer::onFootprintChanged(): num footprint points: %lu",
    layered_costmap_->getFootprint().size());
}

// The method is called when costmap recalculation is required.
// It updates the costmap within its window bounds.
// Inside this method the costmap Sampling is generated and is writing directly
// to the resulting costmap master_grid without any merging with previous layers.
void
SamplingLayer::updateCosts(
  nav2_costmap_2d::Costmap2D & master_grid, int min_i, int min_j,
  int max_i,
  int max_j)
{
  if (!enabled_) {
    return;
  }

  // master_array - is a direct pointer to the resulting master_grid.
  // master_grid - is a resulting costmap combined from all layers.
  // By using this pointer all layers will be overwritten!
  // To work with costmap layer and merge it with other costmap layers,
  // please use costmap_ pointer instead (this is pointer to current
  // costmap layer grid) and then call one of updates methods:
  // - updateWithAddition()
  // - updateWithMax()
  // - updateWithOverwrite()
  // - updateWithTrueOverwrite()
  // In this case using master_array pointer is equal to modifying local costmap_
  // pointer and then calling updateWithTrueOverwrite():
  unsigned char * master_array = master_grid.getCharMap();
  unsigned int size_x = master_grid.getSizeInCellsX(), size_y = master_grid.getSizeInCellsY();

  // {min_i, min_j} - {max_i, max_j} - are update-window coordinates.
  // These variables are used to update the costmap only within this window
  // avoiding the updates of whole area.
  //
  // Fixing window coordinates with map size if necessary.
  min_i = std::max(0, min_i);
  min_j = std::max(0, min_j);
  max_i = std::min(static_cast<int>(size_x), max_i);
  max_j = std::min(static_cast<int>(size_y), max_j);

  // Simply computing one-by-one cost per each cell
  for (int j = min_j; j < max_j; j++) {
    // Reset sampling_index each time when reaching the end of re-calculated window
    // by OY axis.
    for (int i = min_i; i < max_i; i++) {
      int index = master_grid.getIndex(i, j);
      // setting the sampling cost
      if(costmap_[index] == FREE_SPACE)
        costmap_[index] = NOT_IN_GRAPH_SPACE;
    }
  }

  updateWithTrueOverwrite(master_grid,min_i,min_j,max_i,max_j);

  updateSamplePoints(
    master_grid, min_i, min_j, max_i, max_j
  );

  updateConnections(
    master_grid, min_i, min_j, max_i, max_j
  );
}

}  // namespace nav2_sampling_costmap_plugin

// This is the macro allowing a nav2_sampling_costmap_plugin::SamplingLayer class
// to be registered in order to be dynamically loadable of base type nav2_costmap_2d::Layer.
// Usually places in the end of cpp-file where the loadable class written.
#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(nav2_sampling_costmap_plugin::SamplingLayer, nav2_costmap_2d::Layer)
