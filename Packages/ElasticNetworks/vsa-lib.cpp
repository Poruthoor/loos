/*
  This file is part of LOOS.

  LOOS (Lightweight Object-Oriented Structure library)
  Copyright (c) 2010 Tod D. Romo
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



#include "vsa-lib.hpp"

using namespace std;
using namespace loos;


namespace ENM {

  boost::tuple<DoubleMatrix, DoubleMatrix> VSA::eigenDecomp(DoubleMatrix& A, DoubleMatrix& B) {

    DoubleMatrix AA = A.copy();
    DoubleMatrix BB = B.copy();

    f77int itype = 1;
    char jobz = 'V';
    char uplo = 'U';
    char range = 'I';
    f77int n = AA.rows();
    f77int lda = n;
    f77int ldb = n;
    double vl = 0.0;
    double vu = 0.0;
    f77int il = 7;
    f77int iu = n;

    char dpar = 'S';
    double abstol = 2.0 * dlamch_(&dpar);
    //double abstol = -1.0;

    f77int m;
    DoubleMatrix W(n, 1);
    DoubleMatrix Z(n, n);
    f77int ldz = n;

    f77int lwork = -1;
    f77int info;
    double *work = new double[1];

    f77int *iwork = new f77int[5*n];
    f77int *ifail = new f77int[n];

    dsygvx_(&itype, &jobz, &range, &uplo, &n, AA.get(), &lda, BB.get(), &ldb, &vl, &vu, &il, &iu, &abstol, &m, W.get(), Z.get(), &ldz, work, &lwork, iwork, ifail, &info);
    if (info != 0) {
      cerr << "ERROR- dsygvx returned " << info << endl;
      exit(-1);
    }

    lwork = work[0];
    if (verbosity_ > 1)
      std::cerr << "dsygvx requested " << work[0] /(1024.0*1024.0) << " MB\n";

    delete[] work;
    work = new double[lwork];

    dsygvx_(&itype, &jobz, &range, &uplo, &n, AA.get(), &lda, BB.get(), &ldb, &vl, &vu, &il, &iu, &abstol, &m, W.get(), Z.get(), &ldz, work, &lwork, iwork, ifail, &info);
    if (info != 0) {
      cerr << "ERROR- dsygvx returned " << info << endl;
      exit(-1);
    }

    if (m != n-6) {
      cerr << "ERROR- only got " << m << " eigenpairs instead of " << n-6 << endl;
      exit(-10);
    }

    vector<uint> indices = sortedIndex(W);
    W = permuteRows(W, indices);
    Z = permuteColumns(Z, indices);

    boost::tuple<DoubleMatrix, DoubleMatrix> result(W, Z);
    return(result);

  }



  // Mass-weight eigenvectors
  DoubleMatrix VSA::massWeight(DoubleMatrix& U, DoubleMatrix& M) {

    // First, compute the cholesky decomp of M (i.e. it's square-root)
    DoubleMatrix R = M.copy();
    char uplo = 'U';
    f77int n = M.rows();
    f77int lda = n;
    f77int info;
    dpotrf_(&uplo, &n, R.get(), &lda, &info);
    if (info != 0) {
      cerr << "ERROR- dpotrf() returned " << info << endl;
      exit(-1);
    }

    if (debugging_)
      writeAsciiMatrix(prefix_ + "_R.asc", R, meta_, false);

    // Now multiply M * U
    DoubleMatrix UU = U.copy();
    f77int m = U.rows();
    n = U.cols();
    double alpha = 1.0;
    f77int ldb = m;

#if defined(__linux__) || defined(__CYGWIN__) || defined(__FreeBSD__)
    char side = 'L';
    char notrans = 'N';
    char diag = 'N';

    dtrmm_(&side, &uplo, &notrans, &diag, &m, &n, &alpha, R.get(), &lda, UU.get(), &ldb);

#else

    cblas_dtrmm(CblasColMajor, CblasLeft, CblasUpper, CblasNoTrans, CblasNonUnit, m, n, alpha, R.get(), lda, UU.get(), ldb);
#endif

    normalizeColumns(UU);
    return(UU);
  }



  void VSA::solve() {

    if (verbosity_ > 1)
      std::cerr << "Building hessian...\n";
    buildHessian();
    
    uint n = hessian_.cols();
    uint l = subset_size_ * 3;
    
    DoubleMatrix Hss = submatrix(hessian_, Math::Range(0,l), Math::Range(0,l));
    DoubleMatrix Hee = submatrix(hessian_, Math::Range(l, n), Math::Range(l, n));
    DoubleMatrix Hse = submatrix(hessian_, Math::Range(0,l), Math::Range(l, n));
    DoubleMatrix Hes = submatrix(hessian_, Math::Range(l, n), Math::Range(0, l));

    if (debugging_) {
      writeAsciiMatrix(prefix_ + "_H.asc", hessian_, meta_, false);
      writeAsciiMatrix(prefix_ + "_Hss.asc", Hss, meta_, false);
      writeAsciiMatrix(prefix_ + "_Hee.asc", Hee, meta_, false);
      writeAsciiMatrix(prefix_ + "_Hse.asc", Hse, meta_, false);
    }

    if (verbosity_ > 1)
      std::cerr << "Inverting environment hessian...\n";
    DoubleMatrix Heei = Math::invert(Hee);
  
    // Build the effective Hessian
    if (verbosity_ > 1)
      std::cerr << "Computing effective hessian...\n";

    Hssp_ = Hss - Hse * Heei * Hes;

    if (debugging_)
      writeAsciiMatrix(prefix_ + "_Hssp.asc", Hssp_, meta_, false);

    // Shunt in the event of using unit masses...  We can use the SVD to
    // to get the eigenpairs from Hssp
    if (masses_.rows() == 0) {
      Timer<> t;

      if (verbosity_ > 0)
        std::cerr << "Calculating SVD of effective hessian...\n";

      t.start();
      boost::tuple<DoubleMatrix, DoubleMatrix, DoubleMatrix> svdresult = svd(Hssp_);
      t.stop();

      if (verbosity_ > 0)
        std::cerr << "SVD took " << loos::timeAsString(t.elapsed()) << std::endl;

      eigenvecs_ = boost::get<0>(svdresult);
      eigenvals_ = boost::get<1>(svdresult);

      reverseColumns(eigenvecs_);
      reverseRows(eigenvals_);
      return;

    }


    // Build the effective mass matrix
    DoubleMatrix Ms = submatrix(masses_, Math::Range(0, l), Math::Range(0, l));
    DoubleMatrix Me = submatrix(masses_, Math::Range(l, n), Math::Range(l, n));

    if (verbosity_ > 1)
      std::cerr << "Computing effective mass matrix...\n";
    Msp_ = Ms + Hse * Heei * Me * Heei * Hes;

    if (debugging_) {
      writeAsciiMatrix(prefix_ + "_Ms.asc", Ms, meta_, false);
      writeAsciiMatrix(prefix_ + "_Me.asc", Me, meta_, false);
      writeAsciiMatrix(prefix_ + "_Msp.asc", Msp_, meta_, false);
    }

    // Run the eigen-decomposition...
    boost::tuple<DoubleMatrix, DoubleMatrix> eigenpairs;
    Timer<> t;
    if (verbosity_ > 0)
      std::cerr << "Computing eigendecomposition...\n";

    t.start();
    eigenpairs = eigenDecomp(Hssp_, Msp_);
    t.stop();

    if (verbosity_ > 0)
      std::cerr << "Eigendecomposition took " << loos::timeAsString(t.elapsed()) << std::endl;

    eigenvals_ = boost::get<0>(eigenpairs);
    DoubleMatrix Us = boost::get<1>(eigenpairs);

    // Need to mass-weight the eigenvectors so they're orthogonal in R3...
    eigenvecs_ = massWeight(Us, Msp_);
  }


};

