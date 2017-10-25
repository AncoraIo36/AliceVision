// Copyright (c) 2015 Pierre Moulon.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENMVG_SFM_DATA_LOCALBA_HPP
#define OPENMVG_SFM_DATA_LOCALBA_HPP

#include "openMVG/types.hpp"
#include "openMVG/sfm/sfm_data.hpp"
#include "openMVG/tracks/tracks.hpp"
#include "lemon/list_graph.h"
#include "lemon/bfs.h"
#include "openMVG/stl/stlMap.hpp"
#include "openMVG/system/timer.hpp"


namespace openMVG {
namespace sfm {

//------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------
//                                                 TimeSummary      
//------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------

/// Allows to store the time spend in each step of the Local BA.
struct TimeSummary
{
public:
  
  enum EStep {UPDATE_GRAPH, 
              COMPUTE_DISTANCES, 
              CONVERT_DISTANCES2STATES, 
              ADJUSTMENT,
              SAVE_INTRINSICS};
  
  void resetTimer() {_timer.reset();}
  
  void saveTime(EStep step);
  
  bool exportTimes(const std::string& filename);
  
  void showTimes();
  
private:
  
  openMVG::system::Timer _timer;
  
  double _graphUpdating = 0.0;
  double _distancesComputing = 0.0;
  double _distancesConversion = 0.0;
  double _adjusting = 0.0;
  double _saveIntrinsics = 0.0;
  
  double getTotalTime();
};

//------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------
//                                                     LocalBA_Data      
//------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------

/// Contains all the data needed to apply a Local Bundle Adjustment.
class LocalBA_Data
{
  
public:
  
  // -- Constructor
  
  LocalBA_Data(const SfM_Data& sfm_data);
  
  // -- Getters
  
  int getPoseDistance(const IndexT poseId) const;
  
  int getViewDistance(const IndexT viewId) const;
  
  std::set<IndexT> getNewViewsId() const {return _newViewsId;}
  
  std::map<int, std::size_t> getDistancesHistogram() const;
  
  // -- Setters
  
  void setNewViewsId(const std::set<IndexT>& newPosesId) {_newViewsId = newPosesId;}
  
  // -- Methods
  
  /// @brief addIntrinsicsToHistory Add the current intrinsics of the reconstruction in the intrinsics history.
  /// @param[in] sfm_data Contains all the information about the reconstruction, notably current intrinsics
  void addIntrinsicsToHistory(const SfM_Data& sfm_data);
  
  std::size_t addIntrinsicEdgesToTheGraph(const SfM_Data& sfm_data);
  
  void removeIntrinsicEdgesToTheGraph();
  
  
  
  /// @brief exportIntrinsicsHistory Save the history of each intrinsic. It create a file \b K<intrinsic_index>.txt in \a folder.
  /// @param[in] folder The folder in which the \b K*.txt are saved.
  void exportIntrinsicsHistory(const std::string& folder);
  
  /// @brief updateGraphWithNewViews Complete the graph with the newly resected views \a set_newViewsId, or all the posed views if the graph is empty.
  /// @param[in] sfm_data 
  /// @param[in] map_tracksPerView A map giving the tracks for each view
  void updateGraphWithNewViews(
      const SfM_Data& sfm_data, 
      const tracks::TracksPerView& map_tracksPerView);
  
  void drawGraph(const SfM_Data& sfm_data, const std::string& dir);
  void drawGraph(const SfM_Data &sfm_data, const std::string& dir, const std::string& nameComplement);
  
  /// @brief removeViewsToTheGraph Remove some views to the graph. It delete the node and all the incident arcs for each removed view.
  /// @param[in] removedViewsId Set of views index to remove
  /// @return true if the number of removed node is equal to the size of \c removedViewsId
  bool removeViewsToTheGraph(const std::set<IndexT>& removedViewsId);
  
  /// @brief computeDistancesMaps Add the newly resected views 'newViewsIds' into a graph (nodes: cameras, egdes: matching)
  /// and compute the intragraph-distance between these new cameras and all the others.
  void computeDistancesMaps(const SfM_Data& sfm_data);
  
  void convertDistancesToLBAStates(const SfM_Data & sfm_data);
  
  // Define the state of the all parameter of the reconstruction (structure, poses, intrinsics) in the BA:
  enum ELocalBAState { 
    refined,  //< will be adjuted by the BA solver
    constant, //< will be set as constant in the sover
    ignored   //< will not be set into the BA solver
  };
  
  // Get back the 'ELocalBAState' for a specific parameter :
  ELocalBAState getPoseState(const IndexT poseId) const           {return _mapLBAStatePerPoseId.at(poseId);}
  ELocalBAState getIntrinsicState(const IndexT intrinsicId) const {return _mapLBAStatePerIntrinsicId.at(intrinsicId);}
  ELocalBAState getLandmarkState(const IndexT landmarkId) const   {return _mapLBAStatePerLandmarkId.at(landmarkId);
  }
  
  std::size_t getNumberOfConstantAndRefinedCameras();
  
  TimeSummary _timeSummary;
  
private:
  
  /// @brief checkIntrinsicsConsistency Compute, for each camera/intrinsic, the variation of the last \a windowSize values of the focal length.
  /// If it consideres the focal lenght variations as enought constant it updates \a _intrinsicIsConstant.
  /// @details Pipeline:
  /// \b H: the history of all the focal length for a given intrinsic
  /// \b S: the subpart of H including the last \a wondowSize values only.
  /// \b sigma = stddev(S)  
  /// \b sigma_normalized = sigma / (max(H) - min(H))
  /// \c if sigma_normalized < \a stdevPercentageLimit \c then the limit is reached.
  /// @param[in] windowSize Compute the variation on the \a windowSize parameter
  /// @param[in] stdevPercentageLimit The limit is reached when the standard deviation of the \a windowSize values is less than \a stdevPecentageLimit % of the range of all the values.
  void checkIntrinsicsConsistency(const std::size_t windowSize, const double stdevPercentageLimit);
  
  /// @brief selectViewsToAddToTheGraph Return the index of all the posed views not added to the distance graph yet.
  /// It means all the poses if the graph is empty.
  /// @param[in] sfm_data
  /// @return A set of views index
  std::set<IndexT> selectViewsToAddToTheGraph(const SfM_Data& sfm_data);
  
  /// @brief countSharedLandmarksPerImagesPair Extract the images per between the views \c newViewsId and the already recontructed cameras, 
  /// and count the number of commun matches between these pairs.
  /// @param[in] sfm_data
  /// @param[in] map_tracksPerView
  /// @param[in] newViewsId A set with the views index that we want to count matches with resected cameras. 
  /// @return A map giving the number of matches for each images pair.
  std::map<Pair, std::size_t> countSharedLandmarksPerImagesPair(
      const SfM_Data& sfm_data,
      const tracks::TracksPerView& map_tracksPerView,
      const std::set<IndexT>& newViewsId);
  
  
  /// @brief isIntrinsicConstant Giving an intrinsic index and the wished parameter, is return \c true if the limit has alerady been reached, else \c false.
  /// @param[in] intrinsicId The intrinsic index.
  /// @param[in] parameter The \a EIntrinsicParameter to observe.
  /// @return true if the limit is reached, else false
  bool isIntrinsicConstant(const IndexT intrinsicId) const { return _mapIntrinsicIsConstant.at(intrinsicId);}
  
  double getLastFocalLength(const IndexT intrinsicId) const {return _intrinsicsHistory.at(intrinsicId).back().second;}
  
  /// @brief standardDeviation Compute the standard deviation.
  /// @param[in] The values
  /// @return The standard deviation
  template<typename T> 
  double standardDeviation(const std::vector<T>& data);
  
  // ------------------------
  // - Distances data -
  // Local BA needs to know the distance of all the old posed views to the new resected views.
  // The bundle adjustment will be processed on the closest poses only.
  // ------------------------
  
  /// Ensure a minimum number of landmarks in common to consider 2 views as connected in the graph.
  static std::size_t const _kMinNbOfMatches = 100;
  
  /// A graph where nodes are poses and an edge exists when 2 poses shared at least 'kMinNbOfMatches' matches.
  lemon::ListGraph _graph; 
  
  /// A map associating each view index at its node in the graph 'graph_poses'.
  std::map<IndexT, lemon::ListGraph::Node> _mapNodePerViewId;
  std::map<lemon::ListGraph::Node, IndexT> _mapViewIdPerNode;
  std::set<int> _intrinsicEdgesId; // indexed by Edge id.
  
  
  /// Contains all the last resected cameras
  std::set<IndexT> _newViewsId; 
  
  /// Store the graph-distances from the new views (0: is a new view, -1: is not connected to the new views)
  std::map<IndexT, int> _mapDistancePerViewId;
  /// Store the graph-distances from the new poses (0: is a new pose, -1: is not connected to the new poses)
  std::map<IndexT, int> _mapDistancePerPoseId;
  
  /// Store the \c ELocalBAState of each pose in the scene.
  std::map<IndexT, ELocalBAState> _mapLBAStatePerPoseId;
  /// Store the \c ELocalBAState of each intrinsic in the scene.
  std::map<IndexT, ELocalBAState> _mapLBAStatePerIntrinsicId;
  /// Store the \c ELocalBAState of each landmark in the scene.
  std::map<IndexT, ELocalBAState> _mapLBAStatePerLandmarkId;
  
  // ------------------------
  // - Intrinsics data -
  // Local BA needs to know the evolution of all the intrinsics parameters.
  // When camera parameters are enought reffined (no variation) they are set to constant in the BA.
  // ------------------------
  
  /// Save the progression for all the intrinsics parameters
  /// <IntrinsicId, std::vector<std::pair<NumOfPosesCamerasWithThisIntrinsic, FocalLengthHistory>
  /// K1:
  ///   0 1200 
  ///   1 1250
  ///   ...
  /// K2:
  ///   ... 
  using IntrinicsHistory = std::map<IndexT, std::vector<std::pair<std::size_t, double>>>;
  
  /// Backup of the intrinsics focal length values
  IntrinicsHistory _intrinsicsHistory; 
  
  /// Store, for each parameter of each intrinsic, the BA's index from which it has been concidered as constant.
  /// <IntrinsicId, isConsideredAsConstant>
  std::map<IndexT, bool> _mapIntrinsicIsConstant; 
};


//------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------
//                                                 LocalBA_statistics      
//------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------

/// Contain all the information about a Bundle Adjustment loop
struct LocalBA_statistics
{
  LocalBA_statistics(
      const std::set<IndexT>& newlyResectedViewsId = std::set<IndexT>(),
      const std::map<int, std::size_t>& distancesHistogram = std::map<int, std::size_t>()) 
  {
    _newViewsId = newlyResectedViewsId;
    _numCamerasPerDistance = distancesHistogram;
  }
  
  
  // Parameters returned by Ceres:
  double _time = 0.0;                          // spent time to solve the BA (s)
  std::size_t _numSuccessfullIterations = 0;   // number of successfull iterations
  std::size_t _numUnsuccessfullIterations = 0; // number of unsuccessfull iterations
  
  std::size_t _numResidualBlocks = 0;          // num. of resiudal block in the Ceres problem
  
  double _RMSEinitial = 0.0; // sqrt(initial_cost / num_residuals)
  double _RMSEfinal = 0.0;   // sqrt(final_cost / num_residuals)
  
  // Parameters specifically used by Local BA:
  std::size_t _numRefinedPoses = 0;           // number of refined poses among all the estimated views          
  std::size_t _numConstantPoses = 0;          // number of poses set constant in the BA solver
  std::size_t _numIgnoredPoses = 0;           // number of not added poses to the BA solver
  std::size_t _numRefinedIntrinsics = 0;      // num. of refined intrinsics
  std::size_t _numConstantIntrinsics = 0;     // num. of intrinsics set constant in the BA solver
  std::size_t _numIgnoredIntrinsics = 0;      // num. of not added intrinsicsto the BA solver
  std::size_t _numRefinedLandmarks = 0;       // num. of refined landmarks
  std::size_t _numConstantLandmarks = 0;      // num. of landmarks set constant in the BA solver
  std::size_t _numIgnoredLandmarks = 0;       // num. of not added landmarks to the BA solver
  
  std::map<int, std::size_t> _numCamerasPerDistance; // distribution of the cameras for each graph distance
  
  std::set<IndexT> _newViewsId;  // index of the new views added (newly resected)
};



} // namespace sfm
} // namespace openMVG

#endif // OPENMVG_SFM_DATA_LOCAL_HPP
