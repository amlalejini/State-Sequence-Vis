#ifndef STATE_SEQUENCE_VISUALIZATION_H
#define STATE_SEQUENCE_VISUALIZATION_H

#include <string>
#include <iostream>

#include "base/Ptr.h"
#include "web/init.h"
#include "web/JSWrap.h"
#include "web/web.h"
#include "web/d3/dataset.h"
#include "web/d3/visualizations.h"

namespace emp {
namespace web {

// TODO:
//  [ ] Add current category, default category.
//     [ ] Compute y/x domains based on current category.
//  [ ] Dynamic sizing.
//  [ ]
class StateSequenceVisualization : public D3Visualization {
protected:
  /// Convenient structure to keep track of margins.
  struct Margin {
    double top;
    double right;
    double bottom;
    double left;

    Margin(double _top=10.0, double _right=10.0, double _bottom=10.0, double _left=10.0)
      : top(_top), right(_right), bottom(_bottom), left(_left)
    { ; }
  };

  Margin margins;

  // TODO: implement this
  bool dynamic_sizing;

  // Dataset descriptions.
  std::string state_seq_cname;       ///< Column name in dataset that specifies state sequence.
  std::string state_starts_cname;    ///< Column name in dataset that specifies state start values.
  std::string state_durations_cname; ///< Column name in dataset that specifies state duration values.
  std::string category_cname;        ///< Column name in dataset that specifies sequence category.
  std::string seqID_cname;           ///< Column name in the dataset that specifies sequence ID within its category.
  std::string seq_delim;             ///< Sequence delimiter.

  // Coerce naively loaded data into proper format.
  void LoadDataFromCSV(std::string filename) {
    // TODO: add category and sequence IDs
    EM_ASM_ARGS({
      var vis_obj_id = Pointer_stringify($0);
      var filename = Pointer_stringify($1);
      var state_seq_cname = Pointer_stringify($2);
      var state_starts_cname = Pointer_stringify($3);
      var state_durations_cname = Pointer_stringify($4);
      var category_cname = Pointer_stringify($5);
      var seqID_cname = Pointer_stringify($6);
      var seq_delim = Pointer_stringify($7);

      var vis = emp.StateSeqVis[vis_obj_id];
      vis["categories"] = [];
      vis["seqs_by_category"] = {};

      // Max/min sequence values. (for y domain calculations).
      var max_seq = null;
      var min_seq = null;

      var DataAccessor = function(row) {
        var states = row[state_seq_cname].split(seq_delim);
        var state_starts = row[state_starts_cname].split(seq_delim).map(Number);
        var state_durations = row[state_durations_cname].split(seq_delim).map(Number);
        var category = row[category_cname];
        var seq_id = row[seqID_cname];
        var state_sequence = [];
        if (states.length > 0 && max_seq == null) { max_seq = state_starts[0] + state_durations[0]; }
        if (states.length > 0 && min_seq == null) { min_seq = state_starts[0]; }
        for (i = 0; i < states.length; i++) {
          if (max_seq < (state_starts[i] + state_durations[i])) { max_seq = state_starts[i] + state_durations[i]; }
          if (min_seq > state_starts[i]) { min_seq = state_starts[i]; }
          state_sequence.push({state: states[i], duration: state_durations[i], start: state_starts[i]});
        }
        // Build up a list of categories.
        if (vis["categories"].indexOf(category) == -1) {
          vis["seqs_by_category"][category] = [];
          vis["categories"].push(category);
        }
        //   Ensure that seq_ids are unique within a category.
        if (vis["seqs_by_category"][category].indexOf(seq_id) == -1) {
          vis["seqs_by_category"][category].push(seq_id);
        }
        var ret_obj = {};
        ret_obj["category"] = category;
        ret_obj["sequence_id"] = seq_id;
        ret_obj["state_sequence"] = state_sequence;
        return ret_obj;
      };

      // Capture data exists to capture data from callback.
      var DataCallback = function(data) {
        // Capture data, setup data-dependent variables.
        vis["data"] = data;
        vis["domains"]["y"] = ([min_seq, max_seq]);
        vis["domains"]["x"] = ([0, data.length]);

        
      };

      d3.csv(filename, DataAccessor, DataCallback);

    }, GetID().c_str(),
       filename.c_str(),
       state_seq_cname.c_str(),
       state_starts_cname.c_str(),
       state_durations_cname.c_str(),
       category_cname.c_str(),
       seqID_cname.c_str(),
       seq_delim.c_str()
    );

    // Trigger a draw.
    if (this->init) {
      Draw();
    } else {
      this->pending_funcs.Add([this]() { this->Draw(); });
    }
  }

public:
  StateSequenceVisualization() : D3Visualization(1, 1) {
    // Create functions relevant to
    EM_ASM_ARGS({
      console.log("Creating a state sequence vis!");
      var obj_id = Pointer_stringify($0);
      // Create a little JS container to hold everything relevant to this visualization.
      if (!emp.StateSeqVis) { emp.StateSeqVis = {}; }
      emp.StateSeqVis[obj_id] = {};
      // Get a convenient handle on the object container.
      var vis = emp.StateSeqVis[obj_id];
      // Initialize some relevant objects.
      vis["data"] = {};
      vis["categories"] = [];
      vis["seqs_by_category"] = {};
      vis["domains"] = {};

    }, GetID().c_str());
  }

  /// Setup is called automatically when the emp::Document is ready.
  void Setup() {
    EM_ASM_ARGS({
      var vis_obj_id = Pointer_stringify($0);
      // Setup canvasi.
      var svg = d3.select("#"+vis_obj_id);
      svg.selectAll("*").remove();
      var canvas = svg.append("g").attr({"id": "StateSequenceVisualization-canvas-" + vis_obj_id,
                                         "class": "StateSequenceVisualization-canvas"});
      var data_canvas = canvas.append("g").attr({"id": "StateSequenceVisualization-data_canvas-" + vis_obj_id,
                                                 "class": "StateSequenceVisualization-data_canvas"});
    }, GetID().c_str());
    this->init = true;
    this->pending_funcs.Run();
  }

  /// Load data from file given:
  ///   * filename: name/path to data file (expects .csv)
  ///   * states: name of the column that gives state sequence.
  ///   * starts: name of the column that gives the start points for each state in the state sequence.
  ///   * durations: name of the column that gives the durations for each state in the state sequence.
  void LoadDataFromCSV(std::string filename, std::string states,
                       std::string starts, std::string durations,
                       std::string category, std::string seqID,
                       std::string delim="-") {
    state_seq_cname = states;
    state_starts_cname = starts;
    state_durations_cname = durations;
    category_cname = category;
    seqID_cname = seqID;
    seq_delim = delim;
    if (this->init) {
      LoadDataFromCSV(filename);
    } else {
      this->pending_funcs.Add([this, filename]() { this->LoadDataFromCSV(filename); });
    }
  }

  void Draw() {
    std::cout << "Draw!" << std::endl;
    EM_ASM_ARGS({
      var vis_obj_id = Pointer_stringify($0);

      // Get our handle on this visualization's container.
      var vis = emp.StateSeqVis[vis_obj_id];

      // TODO: dynamic sizing.
      // Change width/height?
      $("#"+vis_obj_id).attr({"width": 10, "height": 10});

      // Calculate range.

    }, GetID().c_str());
  }

};

}
}

#endif
