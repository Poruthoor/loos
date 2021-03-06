/*
  Internal water filter library
*/


/*
  This file is part of LOOS.

  LOOS (Lightweight Object-Oriented Structure library)
  Copyright (c) 2008, Tod D. Romo, Alan Grossfield
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


#include <internal-water-filter.hpp>


using namespace std;

namespace loos {
  namespace DensityTools {


    string WaterFilterBox::name(void) const {
      stringstream s;
      s << boost::format("WaterFilterBox(pad=%f)") % pad_;
      return(s.str());
    }
    

    vector<int> WaterFilterBox::filter(const AtomicGroup& solv, const AtomicGroup& prot) {
      bdd_ = boundingBox(prot);
      vector<int> result(solv.size());
  
      AtomicGroup::const_iterator ai;
  
      uint j = 0;
      for (ai = solv.begin(); ai != solv.end(); ++ai) {
        bool flag = true;
        GCoord c = (*ai)->coords();
        for (int i=0; i<3; ++i)
          if (c[i] < bdd_[0][i] || c[i] > bdd_[1][i]) {
            flag = false;
            break;
          }

        result[j++] = flag;
      }
      return(result);
    }


    double WaterFilterBox::volume(void) {
      GCoord v = bdd_[1] - bdd_[0];
      return(v[0] * v[1] * v[2]);
    }

  
    vector<GCoord> WaterFilterBox::boundingBox(const AtomicGroup& grp) {
      vector<GCoord> bdd = grp.boundingBox();
      bdd[0] = bdd[0] - pad_;
      bdd[1] = bdd[1] + pad_;

      return(bdd);
    }


    // --------------------------------------------------------------------------------
    string WaterFilterRadius::name(void) const {
      stringstream s;
      s << boost::format("WaterFilterRadius(radius=%f)") % radius_;
      return(s.str());
    }
    

    vector<int> WaterFilterRadius::filter(const AtomicGroup& solv, const AtomicGroup& prot) {
      bdd_ = boundingBox(prot);
      vector<int> result(solv.size());

      double r2 = radius_ * radius_;
      for (uint j=0; j<solv.size(); ++j) {
        GCoord sc = solv[j]->coords();
        for (uint i=0; i<prot.size(); ++i)
          if (sc.distance2(prot[i]->coords()) <= r2) {
            result[j] = 1;
            break;
          }
      }
      
      return(result);
    }


    double WaterFilterRadius::volume(void) {
      GCoord v = bdd_[1] - bdd_[0];
      return(v[0] * v[1] * v[2]);
    }

  
    vector<GCoord> WaterFilterRadius::boundingBox(const AtomicGroup& grp) {
      vector<GCoord> bdd = grp.boundingBox();
      bdd[0] = bdd[0] - radius_;
      bdd[1] = bdd[1] + radius_;

      return(bdd);
    }


    // --------------------------------------------------------------------------------
    string WaterFilterContacts::name(void) const {
      stringstream s;
      s << boost::format("WaterFilterContacts(radius=%f,contacts=%u)") % radius_ % threshold_;
      return(s.str());
    }
    

    vector<int> WaterFilterContacts::filter(const AtomicGroup& solv, const AtomicGroup& prot) {
      bdd_ = boundingBox(prot);
      vector<int> result(solv.size());

      double r2 = radius_ * radius_;
      for (uint j=0; j<solv.size(); ++j) {
	uint count = 0;
	for (uint i=0; i<prot.size(); ++i) {
	  if (solv[j]->coords().distance2(prot[i]->coords()) <= r2) {
	    ++count;
	    if (count >= threshold_) {
	      result[j] = 1;
	      break;
	    }
	  }
	}
      }

      return(result);
    }


    double WaterFilterContacts::volume(void) {
      GCoord v = bdd_[1] - bdd_[0];
      return(v[0] * v[1] * v[2]);
    }

  
    vector<GCoord> WaterFilterContacts::boundingBox(const AtomicGroup& grp) {
      vector<GCoord> bdd = grp.boundingBox();
      bdd[0] = bdd[0] - radius_;
      bdd[1] = bdd[1] + radius_;

      return(bdd);
    }


    // --------------------------------------------------------------------------------


    string WaterFilterAxis::name(void) const {
      stringstream s;
      s << boost::format("WaterFilterAxis(radius=%f)") % sqrt(radius_);
      return(s.str());
    }


    vector<int> WaterFilterAxis::filter(const AtomicGroup& solv, const AtomicGroup& prot) {
      bdd_ = boundingBox(prot);

      vector<int> result(solv.size());
      AtomicGroup::const_iterator ai;
      uint j = 0;
  
      for (ai = solv.begin(); ai != solv.end(); ++ai) {
        GCoord a = (*ai)->coords();
        if (a.z() < bdd_[0][2] || a.z() > bdd_[1][2]) {
          result[j++] = 0;
          continue;
        }

        // Find the nearest point on the axis to the atom...
        a -= orig_;
        double k = (axis_ * a) / axis_.length2();
        GCoord ah = orig_ + k * axis_;
        GCoord v = (*ai)->coords() - ah;

        // Are we within the radius cutoff?
        double d = v.length2();
        if (d <= radius_)
          result[j++] = true;
        else
          result[j++] = false;
      }
      return(result);
    }

    double WaterFilterAxis::volume(void) {
      return((bdd_[1][2] - bdd_[0][2]) * PI * radius_);
    }


    vector<GCoord> WaterFilterAxis::boundingBox(const AtomicGroup& grp) {

      // Set the principal axis...
      orig_ = grp.centroid();
      vector<GCoord> axes = grp.principalAxes();
      axis_ = axes[0];
      vector<GCoord> bdd = grp.boundingBox();
  
      // Calculate the extents of the box containing the principal axis cylinder...
      double r = sqrt(radius_);
      GCoord lbd = orig_ - axis_ - GCoord(r,r,0.0);
      GCoord ubd = orig_ + axis_ + GCoord(r,r,0.0);

      // Set the z-bounds to the protein bounding box...
      lbd[2] = bdd[0][2];
      ubd[2] = bdd[1][2];

      // Replace...
      bdd[0] = lbd;
      bdd[1] = ubd;

      return(bdd);
    }

    // --------------------------------------------------------------------------------


    string WaterFilterCore::name(void) const {
      stringstream s;
      s << boost::format("WaterFilterCore(radius=%f)") % sqrt(radius_);
      return(s.str());
    }


    GCoord WaterFilterCore::calculateAxis(const AtomicGroup& bundle) 
    {
      if (!bundle.hasBonds())
	throw(runtime_error("WaterFilterCore requires model connectivity (bonds)"));
      
      vector<AtomicGroup> segments = bundle.splitByMolecule();
      GCoord axis(0,0,0);
      
      for (vector<AtomicGroup>::iterator i = segments.begin(); i != segments.end(); ++i) {
	vector<GCoord> axes = (*i).principalAxes();
	if (axes[0].z() < 0.0)
	  axis -= axes[0];
	else
	  axis += axes[0];
      }
      
      axis /= axis.length();
      return(axis);
    }
    


    vector<int> WaterFilterCore::filter(const AtomicGroup& solv, const AtomicGroup& prot) {
      bdd_ = boundingBox(prot);

      vector<int> result(solv.size());
      AtomicGroup::const_iterator ai;
      uint j = 0;

      for (ai = solv.begin(); ai != solv.end(); ++ai) {
        GCoord a = (*ai)->coords();
        if (a.z() < bdd_[0][2] || a.z() > bdd_[1][2]) {
          result[j++] = 0;
          continue;
        }

        // Find the nearest point on the axis to the atom...
        a -= orig_;
        double k = (axis_ * a) / axis_.length2();
        GCoord ah = orig_ + k * axis_;
        GCoord v = (*ai)->coords() - ah;

        // Are we within the radius cutoff?
        double d = v.length2();
        if (d <= radius_)
          result[j++] = true;
        else
          result[j++] = false;
      }
      return(result);
    }

    // TODO: Fix!
    double WaterFilterCore::volume(void) {
      return((bdd_[1][2] - bdd_[0][2]) * PI * radius_);
    }


    vector<GCoord> WaterFilterCore::boundingBox(const AtomicGroup& grp) {

      // Set the principal axis...
      orig_ = grp.centroid();
      axis_ = calculateAxis(grp);
      vector<GCoord> bdd = grp.boundingBox();
  
      // Calculate the extents of the box containing the principal axis cylinder...
      double r = sqrt(radius_);
      GCoord lbd = orig_ - axis_ - GCoord(r,r,0.0);
      GCoord ubd = orig_ + axis_ + GCoord(r,r,0.0);

      // Set the z-bounds to the protein bounding box...
      lbd[2] = bdd[0][2];
      ubd[2] = bdd[1][2];

      // Replace...
      bdd[0] = lbd;
      bdd[1] = ubd;

      return(bdd);
    }


    // --------------------------------------------------------------------------------


    string WaterFilterBlob::name(void) const {
      stringstream s;
      GCoord min = blob_.minCoord();
      GCoord max = blob_.maxCoord();
      DensityGridpoint dim = blob_.gridDims();
      s << "WaterFilterBlob(" << dim << ":" << min << "x" << max << ")";
      return(s.str());
    }


    double WaterFilterBlob::volume(void) {
      if (vol >= 0.0)
        return(vol);

      GCoord d = blob_.gridDelta();
      double delta = d[0] * d[1] * d[2];
      long n = blob_.maxGridIndex();
      long c = 0;
      for (long i=0; i<n; ++i)
        if (blob_(i))
          ++c;

      vol = c * delta;
      return(vol);
    }

    vector<int> WaterFilterBlob::filter(const AtomicGroup& solv, const AtomicGroup& prot) {
      vector<int> result(solv.size());
      AtomicGroup::const_iterator ci;
      uint j = 0;
      for (ci = solv.begin(); ci != solv.end(); ++ci) {
        GCoord c = (*ci)->coords();
        DensityGridpoint probe = blob_.gridpoint(c);
        if (blob_.inRange(probe))
          result[j++] = (blob_(c) != 0);
        else
          result[j++] = 0;
      }

      return(result);
    }


    // This ignores the protein bounding box...
    vector<GCoord> WaterFilterBlob::boundingBox(const AtomicGroup& prot) {
      if (bdd_set)
        return(bdd_);
  
      DensityGridpoint dim = blob_.gridDims();
      DensityGridpoint min = dim;
      DensityGridpoint max(0,0,0);

      for (int k=0; k<dim[2]; ++k)
        for (int j=0; j<dim[1]; ++j)
          for (int i=0; i<dim[0]; ++i) {
            DensityGridpoint probe(i,j,k);
            if (blob_(probe) != 0)
              for (int x=0; x<3; ++x) {
                if (probe[x] < min[x])
                  min[x] = probe[x];
                if (probe[x] > max[x])
                  max[x] = probe[x];
              }
          }

      vector<GCoord> bdd(2);
      bdd[0] = blob_.gridToWorld(min);
      bdd[1] = blob_.gridToWorld(max);
    
      return(bdd);
    }



    // --------------------------------------------------------------------------------


    string ZClippedWaterFilter::name(void) const {
      stringstream s;

      s << boost::format("ZClippedWaterFilter(%s, %f, %f)") % WaterFilterDecorator::name() % zmin_ % zmax_;
      return(s.str());
    }


    vector<int> ZClippedWaterFilter::filter(const AtomicGroup& solv, const AtomicGroup& prot) {
      vector<int> result = WaterFilterDecorator::filter(solv, prot);

      for (uint i=0; i<result.size(); ++i)
        if (result[i]) {
          GCoord c = solv[i]->coords();
          if (c[2] < zmin_ || c[2] > zmax_)
            result[i] = 0;
        }    

      return(result);
    }


    vector<GCoord> ZClippedWaterFilter::boundingBox(const AtomicGroup& grp) {
      vector<GCoord> bdd = WaterFilterDecorator::boundingBox(grp);
      bdd[0][2] = zmin_;
      bdd[1][2] = zmax_;

      return(bdd);
    }

    // --------------------------------------------------------------------------------

    string BulkedWaterFilter::name(void) const {
      stringstream s;

      s << boost::format("BulkedWaterFilter(%s, %f, %f, %f)") % WaterFilterDecorator::name() % pad_ % zmin_ % zmax_;
      return(s.str());
    }


    vector<int> BulkedWaterFilter::filter(const AtomicGroup& solv, const AtomicGroup& prot) {
      vector<int> result = WaterFilterDecorator::filter(solv, prot);
      vector<GCoord> bdd = boundingBox(prot);

      for (uint i=0; i<result.size(); ++i)
        if (!result[i]) {
          GCoord c = solv[i]->coords();
          if ( ((c[0] >= bdd[0][0] && c[0] <= bdd[1][0]) &&
                (c[1] >= bdd[0][1] && c[1] <= bdd[1][1]) &&
                (c[2] >= bdd[0][2] && c[2] <= zmin_))
               ||
               ((c[0] >= bdd[0][0] && c[0] <= bdd[1][0]) &&
                (c[1] >= bdd[0][1] && c[1] <= bdd[1][1]) &&
                (c[2] <= bdd[1][2] && c[2] >= zmax_)) )
            result[i] = true;
        }    

      return(result);
    }


    vector<GCoord> BulkedWaterFilter::boundingBox(const AtomicGroup& grp) {
      vector<GCoord> bdd = grp.boundingBox();

      bdd[0] -= pad_;
      bdd[1] += pad_;
      return(bdd);
    }

  };


};
