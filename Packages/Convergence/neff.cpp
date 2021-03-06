/*
  neff

  Based on Zhang, Bhatt, and Zuckerman; JCTC, DOI: 10.1021/ct1002384
  and code provided by the Zuckerman Lab
  (http://www.ccbb.pitt.edu/Faculty/zuckerman/software.html)



  Computes effective sample size given an assignment file and a state file
*/



/*

  This file is part of LOOS.

  LOOS (Lightweight Object-Oriented Structure library)
  Copyright (c) 2010, Tod D. Romo
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
#include <boost/format.hpp>
#include <limits>

using namespace loos;
using namespace std;


typedef vector<uint>             vUint;
typedef vector<vUint>           vvUint;


const double stddev_tol = 1e-6;


vvUint readStates(const string& fname) {
  
  ifstream ifs(fname.c_str());
  if (ifs.fail()) {
      cerr << "Error- cannot open " << fname << endl;
      exit(-1);
  }
  
  vvUint states;

  int n;
  string dummy;
  getline(ifs, dummy);
  ifs >> n;
  if (n <= 0) {
    cerr << boost::format("Error- bad number of states (%d).\n") % n;
    exit(-10);
  }

  while (n-- > 0) {
    int m;
    ifs >> m;
    if (m <= 0) {
      cerr << boost::format("Error- bad number of bins (%d).\n") % m;
      exit(-10);
    }
    vUint list;
    while (m-- > 0) {
      uint s;
      ifs >> s;
      list.push_back(s);
    }
    states.push_back(list);
  }
  
  return(states);
}


vector<uint> readAssignments(const string& fname) {
  ifstream ifs(fname.c_str());
  if (ifs.fail()) {
      cerr << "Error- cannot open " << fname << endl;
      exit(-1);
  }
  
  return(readVector<uint>(ifs));
}



vUint mapStates(const vvUint& states) {
  uint maxbin = 0;
  for (vvUint::const_iterator j = states.begin(); j != states.end(); ++j)
    for (vUint::const_iterator i = j->begin(); i != j->end(); ++i)
      if (*i > maxbin)
        maxbin = *i;

  vUint binmap(maxbin, 0);
  for (uint j=0; j<states.size(); ++j)
    for (vUint::const_iterator i = states[j].begin(); i != states[j].end(); ++i)
      binmap[*i] = j;

  return(binmap);
}


vector<double> rowAvg(const DoubleMatrix& M) {
  vector<double> avg(M.rows(), 0.0);

  for (uint j=0; j<M.rows(); ++j) {
    for (uint i=0; i<M.cols(); ++i)
      avg[j] += M(j, i);
    avg[j] /= M.cols();
  }

  return(avg);
}


vector<double> rowStd(const DoubleMatrix& M, const vector<double>& means) {
  vector<double> dev(M.rows(), 0.0);

  for (uint j=0; j<M.rows(); ++j) {
    for (uint i=0; i<M.cols(); ++i) {
      double d = M(j, i) - means[j];
      dev[j] += d*d;
    }
    dev[j] = sqrt(dev[j] / (M.cols() - 1));
  }

  return(dev);
}


string fullHelpMessage(void) {
  string msg =
    "\n"
    "SYNOPSIS\n"
    "\tDetermine the effective sample size of a simulation\n"
    "\n"
    "DESCRIPTION\n"
    "\n"
    "\tThis tool determines the effective sample size (Neff) as described in\n"
    "Zhang, Batt, and Zuckerman, JCTC (2010) 6:3048-57.\n"
    "\n"
    "EXAMPLES\n"
    "\n"
    "\tneff assignments.asc zuckerman.states 0.1\n"
    "This example determines the Neff given the structural histogram assigments\n"
    "in assignments.asc, the clustering in zuckerman.states, and a bin-probability of 0.1\n"
    "\n"
    "NOTES\n"
    "\n"
    "\tThe partition_size should match the bin-probability used in\n"
    "generating the structural histogram (i.e. ufidpick)\n"
    "\n"
    "SEE ALSO\n"
    "\tufidpick, assign_frames, hierarchy, effsize.pl\n";

  return(msg);
}



int main(int argc, char *argv[]) {
  
  if (argc != 4) {
    cerr << "Usage- " << argv[0] << " assignments states partition_size\n";
    cerr << fullHelpMessage();
    exit(-1);
  }

  string hdr = invocationHeader(argc, argv);

  int k = 1;
  vector<uint> assignments = readAssignments(argv[k++]);

  vvUint states = readStates(argv[k++]);
  uint N = states.size();

  uint partition_size = atoi(argv[k++]);

  uint nparts = floor(assignments.size() / partition_size);
  DoubleMatrix M(N, nparts);
  vUint binmap = mapStates(states);

  for (uint i=0; i<nparts; ++i) {
    for (uint k=0; k<partition_size; ++k) {
      uint col = i*partition_size + k;
      uint bin = binmap[assignments[col]];
      if (bin > N) {
        cerr << "Error- internal error, bin=" << bin << ", N=" << N << endl;
        exit(-20);
      }
      M(bin, i) += 1;
    }
    for (uint j=0; j<N; ++j)
      M(j, i) /= partition_size;
  }

  vector<double> means = rowAvg(M);
  vector<double> devs = rowStd(M, means);

  double min_neff = numeric_limits<double>::max();
  for (uint j=0; j<N; ++j) {
    double neff = (1.0 - means[j]) * means[j] / (devs[j] * devs[j]);
    cout << boost::format("Estimated effective sample size from state %d = %f\n") % j % neff;
    if (neff < min_neff)
      min_neff = neff;
  }

  double total = min_neff * nparts;

  cout << boost::format("Segment effective sample size = %f\n") % min_neff;
  cout << boost::format("Trajectory effective sample size = %f\n") % total;
}
