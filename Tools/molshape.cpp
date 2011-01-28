/*
  molwatch
  
  (c) 2009 Tod D. Romo, Grossfield Lab
  Department of Biochemistry
  University of Rochster School of Medicine and Dentistry

  Computes size/shape/positional information for a selection over time...
*/



/*

  This file is part of LOOS.

  LOOS (Lightweight Object-Oriented Structure library)
  Copyright (c) 2008-2009, Tod D. Romo
  Department of Biochemistry and Biophysics
  School of Medicine & Dentistry, University of Rochester

  This package (LOOS) is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation under version 3 of the License.

  This package is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/



#include <loos.hpp>
#include <boost/program_options.hpp>
#include <boost/format.hpp>

using namespace std;
using namespace loos;
namespace po = boost::program_options;

string model_name, traj_name;
string selection;
int verbosity = 0;
bool split_by_mol;
bool split_by_segid;
bool zabs;



void fullHelp(void) {
}



void parseOptions(int argc, char *argv[]) {
  try {

    po::options_description generic("Allowed options");
    generic.add_options()
      ("help,h", "Produce this help message")
      ("molecule,m", po::value<bool>(&split_by_mol)->default_value(false), "Split by molecule")
      ("segid,s", po::value<bool>(&split_by_segid)->default_value(false), "Split by segid")
      ("abs,a", po::value<bool>(&zabs)->default_value(false), "Use absolute Z-value")
      ("fullhelp", "Even more help");


    po::options_description hidden("Hidden options");
    hidden.add_options()
      ("model", po::value<string>(&model_name), "Model filename")
      ("traj", po::value<string>(&traj_name), "Trajectory filename")
      ("selection", po::value<string>(&selection), "Selection to compute over");
    
    po::options_description command_line;
    command_line.add(generic).add(hidden);
    
    po::positional_options_description p;
    p.add("model", 1);
    p.add("traj", 1);
    p.add("selection", 1);

    
    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
              options(command_line).positional(p).run(), vm);
    po::notify(vm);

    if (vm.count("help") || vm.count("fullhelp") || !(vm.count("model") && vm.count("traj") && vm.count("selection"))) {
      cerr << "Usage- " << argv[0] << " [options] model-name trajectory-name selection\n";
      cerr << generic;
      if (vm.count("fullhelp"))
        fullHelp();
      exit(-1);
    }

  }
  catch(exception& e) {
    cerr << "Error - " << e.what() << endl;
    exit(-1);
  }
}



string split(const loos::Coord<double>& g) {
  stringstream ss;

  ss << setw(10) << g[0] << " " << g[1] << " " << g[2];
  return(ss.str());
}


void modifyZ(AtomicGroup& grp) {

  for (AtomicGroup::iterator i = grp.begin(); i != grp.end(); ++i) {
    GCoord c = (*i)->coords();
    c.z() = abs(c.z());
    (*i)->coords(c);
  }
}


int main(int argc, char *argv[]) {
  string hdr = invocationHeader(argc, argv);

  parseOptions(argc, argv);

  cout << "# " << hdr << endl;
  
  AtomicGroup model = createSystem(model_name);
  AtomicGroup subset = selectAtoms(model, selection);

  vector<AtomicGroup> objects;
  if (split_by_mol)
    objects = subset.splitByMolecule();
  else if (split_by_segid)
    objects = subset.splitByUniqueSegid();
  else
    objects.push_back(subset);

  cout << "# 1 2  3  4  5   6    7    8    9    10      11  12  13  14:16 17:19 20:22\n";
  cout << "# t cX cY cZ Vol BoxX BoxY BoxZ rgyr pA1/pA2 pA1 pA2 pA3 (pV1) (pV2) (pV3)\n";

  
  pTraj traj = createTrajectory(traj_name, model);

  uint t=0;
  while (traj->readFrame()) {
    traj->updateGroupCoords(subset);
    if (zabs)
      modifyZ(subset);
    for (uint i=0; i<objects.size(); ++i) {
      GCoord c = objects[i].centroid();
      vector<GCoord> bdd = objects[i].boundingBox();
      GCoord box = bdd[1] - bdd[0];
      double vol = box[0] * box[1] * box[2];
      vector<GCoord> paxes = objects[i].principalAxes();
      double ratio = paxes[3][0] / paxes[3][1];
      double rgyr = objects[i].radiusOfGyration();
      
      cout << setw(10) << t <<  " " << split(c) << " " << vol << " " << split(box) << " " << rgyr << " ";
      cout << ratio << " " << split(paxes[3]) << " " << split(paxes[0]) << " " << split(paxes[1]) << " " << split(paxes[2]) << endl;
    }
    
    ++t;
  }
}
