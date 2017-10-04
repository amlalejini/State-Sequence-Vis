#ifndef STATE_SEQUENCE_VISUALIZATION_H
#define STATE_SEQUENCE_VISUALIZATION_H

#include <string>
#include <iostream>
#include <chrono>

#include "base/Ptr.h"
#include "base/vector.h"
#include "web/init.h"
#include "web/JSWrap.h"
#include "web/js_utils.h"
#include "web/web.h"
#include "web/d3/dataset.h"
#include "web/d3/visualizations.h"

namespace emp {
namespace web {

class StateSequenceVisualization : public D3Visualization {
protected:
  /// Convenient structure to keep track of margins. (currently no editing this)
  struct Margin {
    double top;
    double right;
    double bottom;
    double left;

    Margin(double _top=10.0, double _right=10.0, double _bottom=10.0, double _left=50.0)
      : top(_top), right(_right), bottom(_bottom), left(_left)
    { ; }
  };

  Margin margins;

  bool dynamic_width;               ///< Do we dynamically resize visualization width based on parent element width?
  bool data_drawn;                  ///< Have we finished drawing the data yet?
  bool data_loaded;                 ///< Have we finished loading the data yet?

  // Dataset descriptions.
  std::string state_seq_cname;       ///< Column name in dataset that specifies state sequence.
  std::string state_starts_cname;    ///< Column name in dataset that specifies state start values.
  std::string state_durations_cname; ///< Column name in dataset that specifies state duration values.
  std::string category_cname;        ///< Column name in dataset that specifies sequence category.
  std::string seqID_cname;           ///< Column name in the dataset that specifies sequence ID within its category.
  std::string seq_delim;             ///< Sequence delimiter.

  emp::vector<std::string> categories;  ///< Keeps tack of currently available sequence categories.
  std::string cur_category;             ///< Keeps track of current category to display. Validity is checked on Draw().

  // Visualization can dynamically resize, so using my own width/height variables.
  double actual_width;    ///< Actual width of the visualization (may be different from Info()->width).
  double actual_height;   ///< Actual height of the visualization (may be different from Info()->height).

  /// Internal function to set the visualization width to w.
  void SetWidthInternal(double w) { actual_width = w; }

  /// Internal function to set the visualization height to h.
  void SetHeightInternal(double h) { actual_height = h; }

  /// Internal function to set the current category to cat. No validity checking.
  void SetCurrentCategoryInternal(const std::string & cat) { cur_category = cat; }

  /// Internal data callback function. (called when data is done loading)
  void DataCallback() {
    // Get categories out of js and into C++.
    categories.clear();
    emp::pass_vector_to_cpp(categories);
    // default to first category.
    if (cur_category == "") cur_category = categories[0];
    data_loaded = true;
    // Trigger a draw.
    Draw();
  }

  /// Internal load data from CSV function given the filename where data is stored.
  void LoadDataFromCSV(std::string filename) {
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
      vis["domains"] = {};

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

        // Build up a list of categories.
        if (vis["categories"].indexOf(category) == -1) {
          vis["seqs_by_category"][category] = [];
          vis["domains"][category] = ({"y": ([state_starts[0], state_starts[0] + state_durations[0]]),
                                      "x": ([0, 0])});
          vis["categories"].push(category);
        }
        //   Ensure that seq_ids are unique within a category.
        if (vis["seqs_by_category"][category].indexOf(seq_id) == -1) {
          vis["seqs_by_category"][category].push(seq_id);
        }
        vis["domains"][category]["x"][1] += 1;
        for (i = 0; i < states.length; i++) {
          if (vis["domains"][category]["y"][1] < (state_starts[i] + state_durations[i])) {
            vis["domains"][category]["y"][1] = state_starts[i] + state_durations[i];
          }
          if (vis["domains"][category]["y"][0] > state_starts[i]) {
            vis["domains"][category]["y"][0] = state_starts[i];
          }
          state_sequence.push({state: states[i], duration: state_durations[i], start: state_starts[i]});
        }
        var ret_obj = {};
        ret_obj["category"] = category;
        ret_obj["sequence_id"] = seq_id;
        ret_obj["state_sequence"] = state_sequence;
        return ret_obj;
      };

      // Capture data exists to capture data from callback.
      var DataCallback = function(data) {
        if (vis["categories"].length == 0) {
          alert("Can't find categories for " + vis_obj_id + "! It's likely nothing will work");
        }
        // Capture data, setup data-dependent variables.
        vis["data"] = data;

        emp_i.__outgoing_array = vis["categories"];
        emp[vis_obj_id+"_data_callback"]();
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
  }

  /// Internal draw function.
  void Draw() {
    EM_ASM_ARGS({
      var vis_obj_id = Pointer_stringify($0);
      var GetHeight = emp[vis_obj_id+"_get_height"];
      var GetWidth = emp[vis_obj_id+"_get_width"];
      var SetCurrentCategory = emp[vis_obj_id+"_set_current_category"];

      var cur_category = Pointer_stringify($1);
      var margins = ({top: $2, right: $3, bottom: $4, left: $5});

      // Get our handle on this visualization's container.
      var vis = emp.StateSeqVis[vis_obj_id];

      // Check on cur category (make sure it's valid).
      if (vis["categories"].indexOf(cur_category) == -1) {
        alert("Failed to find category: " + cur_category + "\nDisplaying default: " + vis["categories"][0]);
        cur_category = vis["categories"][0];
        SetCurrentCategory(cur_category);
      }

      var vis_width = GetWidth();
      var vis_height = GetHeight();
      var canvas_width = vis_width - margins.left - margins.right;
      var canvas_height = vis_height - margins.top - margins.bottom;

      var x_domain = vis["domains"][cur_category]["x"];
      var x_range = ([0, canvas_width]);
      var y_domain = vis["domains"][cur_category]["y"];
      var y_range = ([0, canvas_height]);

      var xScale = d3.scale.linear().domain(x_domain).range(x_range);
      var yScale = d3.scale.linear().domain(y_domain).range(y_range);

      var svg = d3.select("#"+vis_obj_id);
      svg.attr({"width": vis_width, "height": vis_height});
      var canvas = d3.select("#StateSequenceVisualization-canvas-" + vis_obj_id);
      canvas.attr({"transform": "translate(" + margins.left + "," + margins.top + ")"});

      // Attach axes.
      canvas.selectAll(".y_axis").remove();
      canvas.selectAll(".x_axis").remove();
      var xAxis = d3.svg.axis().scale(xScale).tickValues([]).orient("top");
      var yAxis = d3.svg.axis().scale(yScale).orient("left");
      canvas.append("g").attr({"class": "axis y_axis",
                               "id": "StateSequenceVisualization-y_axis-" + vis_obj_id
                             }).call(yAxis);
      canvas.append("g").attr({"class": "axis x_axis",
                               "id": "StateSequenceVisualization-x_axis-" + vis_obj_id
                              }).call(xAxis);
      var axes = canvas.selectAll(".axis");
      axes.selectAll("path").style({"fill": "none", "stroke": "black", "shape-rendering": "crispEdges"});
      axes.selectAll("text").style({"font-family": "sans-serif", "font-size": "10px"});

      var data_canvas = d3.select("#StateSequenceVisualization-data_canvas-" + vis_obj_id);
      data_canvas.selectAll("*").remove();
      var filtered_data = vis["data"].filter(function(d) { return d.category == cur_category; });

      var sequences = data_canvas.selectAll("g").data(filtered_data);
      sequences.enter().append("g");
      sequences.attr({"class": "state-sequence-" + vis_obj_id,
                      "id": function(d) { return d.sequence_id + "_" + vis_obj_id; },
                      "transform": function(seq, i) {
                        var x_trans = xScale(i);
                        var y_trans = yScale(0);
                        return "translate(" + x_trans + "," + y_trans + ")";
                      }
                    });

      sequences.each(function(seq, i) {
        var states = d3.select(this).selectAll("rect").data(seq.state_sequence);
        states.enter().append("rect");
        states.attr({"class": function(info, i) { return info.state; },
                     "state": function(info, i) { return info.state; },
                     "start": function(info, i) { return info.start; },
                     "duration": function(info, i) { return info.duration; },
                     "transform": function(info, i) {
                       return "translate(0," + yScale(info.start) + ")";
                     },
                     "height": function(info, i) { return yScale(info.duration) - 0.5; },
                     "width": xScale(0.9),
                     "fill": "grey"
                   });
      });

    }, GetID().c_str(),
       cur_category.c_str(),
       margins.top,
       margins.right,
       margins.bottom,
       margins.left
    );
    data_drawn = true;
  }

  /// Internal function used to resize the visualization to dimensions specified by
  /// actual_width and actual_height.
  void Resize() {
    if (!data_drawn) return;
    EM_ASM_ARGS({
      var vis_obj_id = Pointer_stringify($0);
      var GetHeight = emp[vis_obj_id+"_get_height"];
      var GetWidth = emp[vis_obj_id+"_get_width"];

      var cur_category = Pointer_stringify($1);
      var margins = ({top: $2, right: $3, bottom: $4, left: $5});

      // Get our handle on this visualization's container.
      var vis = emp.StateSeqVis[vis_obj_id];

      var vis_width = GetWidth();
      var vis_height = GetHeight();
      var canvas_width = vis_width - margins.left - margins.right;
      var canvas_height = vis_height - margins.top - margins.bottom;

      var x_domain = vis["domains"][cur_category]["x"];
      var x_range = ([0, canvas_width]);
      var y_domain = vis["domains"][cur_category]["y"];
      var y_range = ([0, canvas_height]);

      var xScale = d3.scale.linear().domain(x_domain).range(x_range);
      var yScale = d3.scale.linear().domain(y_domain).range(y_range);

      var svg = d3.select("#"+vis_obj_id);
      svg.attr({"width": vis_width, "height": vis_height});
      var canvas = d3.select("#StateSequenceVisualization-canvas-" + vis_obj_id);
      canvas.attr({"transform": "translate(" + margins.left + "," + margins.top + ")"});

      // Remove and re-attach axes.
      canvas.selectAll(".y_axis").remove();
      canvas.selectAll(".x_axis").remove();
      var xAxis = d3.svg.axis().scale(xScale).tickValues([]).orient("top");
      var yAxis = d3.svg.axis().scale(yScale).orient("left");
      canvas.append("g").attr({"class": "axis y_axis",
                               "id": "StateSequenceVisualization-y_axis-" + vis_obj_id
                             }).call(yAxis);
      canvas.append("g").attr({"class": "axis x_axis",
                               "id": "StateSequenceVisualization-x_axis-" + vis_obj_id
                              }).call(xAxis);
      var axes = canvas.selectAll(".axis");
      axes.selectAll("path").style({"fill": "none", "stroke": "black", "shape-rendering": "crispEdges"});
      axes.selectAll("text").style({"font-family": "sans-serif", "font-size": "10px"});


      var data_canvas = d3.select("#StateSequenceVisualization-data_canvas-" + vis_obj_id);
      var sequences = data_canvas.selectAll(".state-sequence-"+vis_obj_id);
      sequences.attr({"transform": function(seq, i) {
                        var x_trans = xScale(i);
                        var y_trans = yScale(0);
                        return "translate(" + x_trans + "," + y_trans + ")";
                      }
                    });

      sequences.each(function(seq, i) {
        var states = d3.select(this).selectAll("rect");
        states.attr({"transform": function(info, i) {
                       return "translate(0," + yScale(info.start) + ")";
                     },
                     "height": function(info, i) { return yScale(info.duration) - 0.5; },
                     "width": xScale(0.9)
                   });
      });

    }, GetID().c_str(),
       cur_category.c_str(),
       margins.top,
       margins.right,
       margins.bottom,
       margins.left
    );
  }

public:
  StateSequenceVisualization(double _width, double _height, bool _dynamic_width=false)
    : D3Visualization(_width, _height), margins(), dynamic_width(_dynamic_width),
      data_drawn(false), data_loaded(false),
      state_seq_cname(), state_starts_cname(), state_durations_cname(), category_cname(),
      seqID_cname(), seq_delim(), categories(), cur_category(""),
      actual_width(_width), actual_height(_height)
  {
    EM_ASM_ARGS({
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
    JSWrap([this]() { this->DataCallback(); }, GetID() + "_data_callback");
    JSWrap([this](double w) { this->SetWidthInternal(w); }, GetID() + "_set_width");
    JSWrap([this](double h) { this->SetHeightInternal(h); }, GetID() + "_set_height");
    JSWrap([this]() { return this->GetForRealWidth(); }, GetID() + "_get_width");
    JSWrap([this]() { return this->GetForRealHeight(); }, GetID() + "_get_height");
    JSWrap([this]() { return this->IsDynamicWidth(); }, GetID() + "_is_dynamic_width");
    JSWrap([this]() { this->Resize(); }, GetID() + "_resize");
    JSWrap([this](std::string cat) { this->SetCurrentCategoryInternal(cat); }, GetID() + "_set_current_category");

    EM_ASM_ARGS({
      var vis_obj_id = Pointer_stringify($0);
      var GetHeight = emp[vis_obj_id+"_get_height"];
      var GetWidth = emp[vis_obj_id+"_get_width"];
      var SetHeight = emp[vis_obj_id+"_set_height"];
      var SetWidth = emp[vis_obj_id+"_set_width"];
      var IsDynamicWidth = emp[vis_obj_id+"_is_dynamic_width"];
      var Resize = emp[vis_obj_id+"_resize"];
      // Setup canvasi.
      var svg = d3.select("#"+vis_obj_id);
      // Set size. (dynamic sizing vs. static)
      if (IsDynamicWidth()) {
        var w = svg[0][0].parentNode.clientWidth;
        SetWidth(w);
      }
      svg.attr({"width": GetWidth(), "height": GetHeight()});

      svg.selectAll("*").remove();
      var canvas = svg.append("g").attr({"id": "StateSequenceVisualization-canvas-" + vis_obj_id,
                                         "class": "StateSequenceVisualization-canvas"});
      var data_canvas = canvas.append("g").attr({"id": "StateSequenceVisualization-data_canvas-" + vis_obj_id,
                                                 "class": "StateSequenceVisualization-data_canvas"});
      // Add a window resize event listener.
      window.addEventListener("resize", function() {
                                          if (IsDynamicWidth()) {
                                            SetWidth(svg[0][0].parentNode.clientWidth);
                                            Resize();
                                          }
                                        }
                              );
    }, GetID().c_str());
    this->init = true;
    this->pending_funcs.Run();
  }

  /// Get the current set of available categories that can be displayed.
  const emp::vector<std::string> & GetCategories() const { return categories; }

  /// Get actual width of the visualization.
  /// Because of dynamic sizing, this may be different from normal GetWidth().
  double GetForRealWidth() const { return actual_width; }

  /// Get actual height of the visualization.
  /// Because of dynamic sizing, this may be different from normal GetHeight().
  double GetForRealHeight() const { return actual_height; }

  /// Retreive the current category. Will return "" if current category has not been
  /// set (by LoadDataFromCSV or SetCurrentCategory functions) yet.
  const std::string & GetCurrentCategory() const { return cur_category; }

  /// Does this visualization have a dynamically sized width?
  bool IsDynamicWidth() const { return dynamic_width; }

  /// Function to set width sizing method (false = static, true = dynamic).
  void SetDynamicWidth(bool val) { dynamic_width = val; }

  /// Set visualization width.
  /// Warning: if you set the visualization width, it will automatically change this visualization
  /// to non-dynamically sizing.
  /// Once width is updated, resizes the visualization.
  void SetWidth(double w) {
    dynamic_width = false;
    SetWidthInternal(w);
    if (data_drawn) Resize();
  }

  /// Set visualization height.
  /// Once the height is set, resizes the visualization.
  void SetHeight(double h) {
    SetHeightInternal(h);
    if (data_drawn) Resize();
  }

  /// Set visualization width/height in one go.
  /// Warning: if you set the visualization width/height this way, it will automatically change this visualization
  /// to non-dynamically sizing.
  /// Once values are set, resize visualization.
  void SetSize(double w, double h) {
    dynamic_width = false;
    SetWidthInternal(w);
    SetHeightInternal(h);
    if (data_drawn) Resize();
  }

  /// Set current category to display.
  /// If data has already been loaded, redraw.
  /// Warning: does not check validity of category here. Validity is checked during drawing.
  void SetCurrentCategory(const std::string & cat) {
    SetCurrentCategoryInternal(cat);
    if (data_loaded) Draw();
  }

  /// Load data from file given:
  ///   * filename: name/path to data file (expects .csv)
  ///   * states: name of the column that gives state sequence.
  ///   * starts: name of the column that gives the start points for each state in the state sequence.
  ///   * durations: name of the column that gives the durations for each state in the state sequence.
  ///   * category: name of the column that gives sequence category (analogous to treatment).
  ///   * seqID: name of column that gives sequence ID (which should be unique within a category).
  ///   * delim: delimiter to used when parsing state/state start/state duration sequences.
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

    data_loaded = false;
    data_drawn = false;

    if (this->init) {
      LoadDataFromCSV(filename);
    } else {
      this->pending_funcs.Add([this, filename]() { this->LoadDataFromCSV(filename); });
    }
  }

  /// Redraw the visualization. Does nothing if data has not been drawn yet. 
  void Redraw() {
    if (!data_drawn) return;
    Draw();
  }

};

}
}

#endif
