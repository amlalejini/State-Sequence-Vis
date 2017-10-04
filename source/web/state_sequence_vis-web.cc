//  This file is part of Project Name
//  Copyright (C) Michigan State University, 2017.
//  Released under the MIT Software license; see doc/LICENSE

#include "web/web.h"

#include "StateSequenceVisualization.h"

namespace UI = emp::web;

UI::Document doc("emp_base");

int main()
{
  UI::StateSequenceVisualization v;
  v.LoadDataFromCSV("data/env_seq_all.dat", "lineage_coded_phenotype_sequence",
                    "lineage_coded_start_updates", "lineage_coded_duration_updates",
                    "treatment", "replicate", "-");

  doc << "<h1>Hello, world!</h1>";
  doc << v;
}
