#ifndef __PLUMED_Matrix_h
#define __PLUMED_Matrix_h
#include <vector>
#include <string>
#include <set>
#include <cassert>
#include "Tools.h"
#include "Log.h"

/// This puts an undescore after after the names of lapack routines
#ifndef F77_FUNC
#define F77_FUNC(name,NAME) name ## _
#endif

extern "C" {
void F77_FUNC(dsyevr,DSYEVR)(const char *jobz, const char *range, const char *uplo, int *n,
                             double *a, int *lda, double *vl, double *vu, int *
                             il, int *iu, double *abstol, int *m, double *w,
                             double *z__, int *ldz, int *isuppz, double *work,
                             int *lwork, int *iwork, int *liwork, int *info);
void F77_FUNC(dgetrf,DGETRF)(int* m, int* n, double* da, int* lda, int* ipiv, int* info);
void F77_FUNC(dgetri,DGETRI)(int* m, double* da, int* lda, int* ipiv, double* work, int* lwork, int* info);
}

namespace PLMD{

/// Calculate the dot product between two vectors 
template <typename T> T dotProduct( const std::vector<T>& A, const std::vector<T>& B ){
   assert( A.size()==B.size() );
   T val; for(unsigned i=0;i<A.size();++i){ val+=A[i]*B[i]; }
   return val;
}

/// Calculate the dot product between a vector and itself
template <typename T> T norm( const std::vector<T>& A ){
   T val; for(unsigned i=0;i<A.size();++i){ val+=A[i]*A[i]; }
   return val;
}

/// This class stores a full matrix and allows one to do some simple matrix operations
template <typename T>
class Matrix {
   /// Matrix matrix multiply
   template <typename U> friend void mult( const Matrix<U>& , const Matrix<U>& , Matrix<U>& );
   /// Matrix times a std::vector 
   template <typename U> friend void mult( const Matrix<U>&, const std::vector<U>& , std::vector<U>& );
   /// std::vector times a Matrix
   template <typename U> friend void mult( const std::vector<U>&, const Matrix<U>&, std::vector<U>& );  
   /// Matrix transpose
   template <typename U> friend void transpose( const Matrix<U>&, Matrix<U>& );
   /// Output the entire matrix on a single line
   template <typename U> friend Log& operator<<(Log&, const Matrix<U>& );
   /// Output the Matrix in matrix form
   template <typename U> friend void matrixOut( Log&, const Matrix<U>& );
   /// Diagonalize a symmetric matrix - returns zero if diagonalization worked
   template <typename U> friend int diagMat( const Matrix<U>& , std::vector<double>& , Matrix<double>& );
   /// Calculate the logarithm of the determinant of a symmetric matrix - returns zero if succesfull
   template <typename U> friend int logdet( const Matrix<U>& , double& );
   /// Invert a matrix (works for both symmetric and assymetric matrices) - returns zero if sucesfull
   template <typename U> friend int Invert( const Matrix<U>& , Matrix<double>& );
   /// Do a cholesky decomposition of a matrix
   template <typename U> friend void cholesky( const Matrix<U>& , Matrix<U>& );
   /// Solve a system of equations using the cholesky decomposition
   template <typename U> friend void chol_elsolve( const Matrix<U>& , const std::vector<U>& , std::vector<U>& );
private:
   /// Number of elements in matrix (nrows*ncols)
   unsigned sz;
   /// Number of rows in matrix
   unsigned rw;
   /// Number of columns in matrix
   unsigned cl;
   /// The data in the matrix
   std::vector<T> data;
public:
   Matrix<T>(const unsigned nr=0, const unsigned nc=0 )  : sz(nr*nc), rw(nr), cl(nc), data(nr*nc) {} 
   Matrix<T>(const Matrix<T>& t) : sz(t.sz), rw(t.rw), cl(t.cl), data(t.data) {} 
   /// Resize the matrix 
   void resize( const unsigned nr, const unsigned nc ){ rw=nr; cl=nc; sz=nr*nc; data.resize(sz); }
   /// Return element i,j of the matrix
   inline T operator () (const unsigned& i, const unsigned& j) const { return data[j+i*cl]; }
   /// Return a referenre to element i,j of the matrix
   inline T& operator () (const unsigned& i, const unsigned& j)      { return data[j+i*cl]; }
   /// Set all elements of the matrix equal to the value of v 
   Matrix<T>& operator=(const T& v){ 
     for(unsigned i=0;i<sz;++i){ data[i]=v; }
     return *this; 
   }
   /// Set the Matrix equal to another Matrix
   Matrix<T>& operator=(const Matrix<T>& m){
     assert( m.rw==rw && m.cl==cl );
     data=m.data; 
     return *this;
   }
   /// Set the Matrix equal to the value of a standard vector - used for readin
   Matrix<T>& operator=(const std::vector<T>& v){
     assert( v.size()==sz );
     for(unsigned i=0;i<sz;++i){ data[i]=v[i]; }
     return *this;
   }
   /// Add v to all elements of the Matrix 
   Matrix<T> operator+=(const T& v){ 
     for(unsigned i=0;i<sz;++i){ data[i]+=v; }
     return *this; 
   }
   /// Matrix addition
   Matrix<T>& operator+=(const Matrix<T>& m){ 
    assert( m.rw==rw && m.cl==cl );
    data+=m.data;
    return *this;
  }
  /// Subtract v from all elements of the Matrix
  Matrix<T> operator-=(const T& v){ 
    for(unsigned i=0;i<sz;++i){ data-=v; }
    return *this; 
  }
  /// Matrix subtraction
  Matrix<T>& operator-=(const Matrix<T>& m){
    assert( m.rw==rw && m.cl==cl );
    data-=m.data; 
    return *this;
  }
  /// Test if the matrix is symmetric or not
  unsigned isSymmetric() const { 
     if (rw!=cl){ return 0; }
     unsigned sym=1; 
     for(unsigned i=1;i<rw;++i) for(unsigned j=0;j<i;++j) if( data[i+j*cl]!=data[j+i*cl] ){ sym=0; break; }  
     return sym;
  }
};

template <typename T> void mult( const Matrix<T>& A , const Matrix<T>& B , Matrix<T>& C ){
  assert(A.cl==B.rw);
  if( A.rw !=C.rw  || B.cl !=C.cl ){ C.resize( A.rw , B.cl ); } C= (T) 0; 
  for(unsigned i=0;i<A.rw;++i) for(unsigned j=0;j<B.cl;++j) for (unsigned k=0; k<A.cl; ++k) C(i,j)+=A(i,k)*B(k,j); 
}

template <typename T> void mult( const Matrix<T>& A, const std::vector<T>& B, std::vector<T>& C){
  assert( A.cl==B.size() );
  if( C.size()!=A.rw  ){ C.resize(A.rw); } 
  for(unsigned i=0;i<A.rw;++i){ C[i]= (T) 0; }
  for(unsigned i=0;i<A.rw;++i) for(unsigned k=0;k<A.cl;++k) C[i]+=A(i,k)*B[k] ;
}

template <typename T> void mult( const std::vector<T>& A, const Matrix<T>& B, std::vector<T>& C){
  assert( B.rw==A.size() );
  if( C.size()!=B.cl ){C.resize( B.cl );} 
  for(unsigned i=0;i<B.cl;++i){ C[i]= (T) 0; }
  for(unsigned i=0;i<B.cl;++i) for(unsigned k=0;k<B.rw;++k) C[i]+=A[k]*B(k,i); 
}

template <typename T> void transpose( const Matrix<T>& A, Matrix<T>& AT ){
  if( A.rw!=AT.cl || A.cl!=AT.rw ) AT.resize( A.cl , A.rw );
  for(unsigned i=0;i<A.cl;++i) for(unsigned j=0;j<A.rw;++j) AT(i,j)=A(j,i); 
}

template <typename T> Log& operator<<(Log& ostr, const Matrix<T>& mat){
   for(unsigned i=0;i<mat.sz;++i) ostr<<mat.data[i]<<" ";
   return ostr;
}

template <typename T> void matrixOut( Log& ostr, const Matrix<T>& mat){
   for(unsigned i=0;i<mat.rw;++i){
      for(unsigned j=0;j<mat.cl;++j){ ostr<<mat(i,j)<<" "; }
      ostr<<"\n";
   }
   return;
}

template <typename T> int diagMat( const Matrix<T>& A, std::vector<double>& eigenvals, Matrix<double>& eigenvecs ){

   // Check matrix is square and symmetric 
   assert( A.rw==A.cl ); assert( A.isSymmetric()==1 );
   double *da=new double[A.sz]; unsigned k=0; double *evals=new double[ A.cl ];
   // Transfer the matrix to the local array
   for (unsigned i=0; i<A.cl; ++i) for (unsigned j=0; j<A.rw; ++j) da[k++]=(double) A(j,i);

   int n=A.cl; int lwork=-1, liwork=-1, m, info, one=1;
   double *work=new double[A.cl]; int *iwork=new int[A.cl];
   double vl, vu, abstol=0.0;
   int* isup=new int[2*A.cl]; double *evecs=new double[A.sz];

   F77_FUNC(dsyevr,DSYEVR)("V", "I", "U", &n, da, &n, &vl, &vu, &one, &n ,
                            &abstol, &m, evals, evecs, &n,
                            isup, work, &lwork, iwork, &liwork, &info);
   if (info!=0) return info;

   // Retrieve correct sizes for work and iwork then reallocate
   liwork=iwork[0]; delete [] iwork; iwork=new int[liwork];
   lwork=(int) work[0]; delete [] work; work=new double[lwork];

   F77_FUNC(dsyevr,DSYEVR)("V", "I", "U", &n, da, &n, &vl, &vu, &one, &n ,
                            &abstol, &m, evals, evecs, &n,
                            isup, work, &lwork, iwork, &liwork, &info);
   if (info!=0) return info;

   if( eigenvals.size()!=A.cl ){ eigenvals.resize( A.cl ); }
   if( eigenvecs.rw!=A.rw || eigenvecs.cl!=A.cl ){ eigenvecs.resize( A.rw, A.cl ); }
   k=0;
   for(unsigned i=0;i<A.cl;++i){
      eigenvals[i]=evals[i];
      // N.B. For ease of producing projectors we store the eigenvectors
      // ROW-WISE in the eigenvectors matrix.  The first index is the 
      // eigenvector number and the second the component
      for(unsigned j=0;j<A.rw;++j){ eigenvecs(i,j)=evecs[k++]; }
   }

   // Deallocate all the memory used by the various arrays
   delete[] da; delete [] work; delete [] evals; delete[] evecs; delete [] iwork; delete [] isup;
   return 0;
}

template <typename T> int Invert( const Matrix<T>& A, Matrix<double>& inverse ){

  if( A.isSymmetric()==1 ){
     // GAT -- I only ever use symmetric matrices so I can invert them like this.
     // I choose to do this as I have had problems with the more general way of doing this that 
     // is implemented below. 
     std::vector<double> eval(A.rw); Matrix<double> evec(A.rw,A.cl), tevec(A.rw,A.cl);
     int err; err=diagMat( A, eval, evec );
     if(err!=0) return err;
     for (unsigned i=0; i<A.rw; ++i) for (unsigned j=0; j<A.cl; ++j) tevec(i,j)=evec(j,i)/eval[j];
     mult(tevec,evec,inverse);
  } else {
     double *da=new double[A.sz]; int *ipiv=new int[A.cl];
     unsigned k=0; int n=A.rw, info;
     for(unsigned i=0;i<A.cl;++i) for(unsigned j=0;j<A.rw;++j) da[k++]=(double) A(j,i);

     F77_FUNC(dgetrf, DGETRF)(&n,&n,da,&n,ipiv,&info);
     if(info!=0) return info;

     int lwork=-1; double* work=new double[A.cl];
     F77_FUNC(dgetri, DGETRI)(&n,da,&n,ipiv,work,&lwork,&info);
     if(info!=0) return info;

     lwork=(int) work[0]; delete [] work; work=new double[lwork];
     F77_FUNC(dgetri, DGETRI)(&n,da,&n,ipiv,work,&lwork,&info);
     if(info!=0) return info;

     if( inverse.cl!=A.cl || inverse.rw!=A.rw ){ inverse.resize(A.rw,A.cl); }
     k=0; for(unsigned i=0;i<A.rw;++i) for(unsigned j=0;j<A.cl;++j) inverse(j,i)=da[k++];

     delete[] work; delete[] ipiv;
  }

  return 0;
}

template <typename T> void cholesky( const Matrix<T>& A, Matrix<T>& B ){

   assert( A.rw==A.cl && A.isSymmetric() );
   Matrix<T> L(A.rw ,A.cl); L=0.;
   std::vector<T> D(A.rw,0.);
   for(unsigned i=0; i<A.rw; ++i){
      L(i,i)=(T) 1;
      for (unsigned j=0; j<i; ++j){
         L(i,j)=A(i,j);
         for (unsigned k=0; k<j; ++k) L(i,j)-=L(i,k)*L(j,k)*D[k];
         if (D[j]!=0.) L(i,j)/=D[j]; else L(i,j)=(T) 0;
      }
      D[i]=A(i,i);
      for (unsigned k=0; k<i; ++k) D[i]-=L(i,k)*L(i,k)*D[k];
   }

   for(unsigned i=0; i<A.rw; ++i) D[i]=(D[i]>0.?sqrt(D[i]):0.);
   if( B.rw!=A.rw || B.cl!=A.cl ){ B.resize( A.rw, A.cl); }
   B=0.; for(unsigned i=0; i<A.rw; ++i) for(unsigned j=0; j<=i; ++j) B(i,j)+=L(i,j)*D[j];
}

template <typename T> void chol_elsolve( const Matrix<T>& M, const std::vector<T>& b, std::vector<T>& y ){

   assert( M.rw==M.cl && M(0,1)==0.0 && b.size()==M.rw );
   if( y.size()!=M.rw ){ y.resize( M.rw ); }
   for(unsigned i=0;i<M.rw;++i){
      y[i]=b[i];
      for(unsigned j=0;j<i;++j) y[i]-=M(i,j)*y[j];
      y[i]*=1.0/M(i,i);
   }
}

template <typename T> int logdet( const Matrix<T>& M, double& ldet ){
   // Check matrix is square and symmetric
   assert( M.rw==M.cl || M.isSymmetric() );

   double *da=new double[M.sz]; unsigned k=0; double *evals=new double[M.cl];
   // Transfer the matrix to the local array
   for (unsigned i=0; i<M.rw; ++i) for (unsigned j=0; j<M.cl; ++j) da[k++]=(double) M(j,i);

   int n=M.cl; int lwork=-1, liwork=-1, info, m, one=1;
   double *work=new double[M.rw]; int *iwork=new int[M.rw];
   double vl, vu, abstol=0.0;
   int* isup=new int[2*M.rw]; double *evecs=new double[M.sz];
   F77_FUNC(dsyevr,DSYEVR)("N", "I", "U", &n, da, &n, &vl, &vu, &one, &n ,
                            &abstol, &m, evals, evecs, &n,
                            isup, work, &lwork, iwork, &liwork, &info);
   if (info!=0) return info;

   // Retrieve correct sizes for work and iwork then reallocate
   lwork=(int) work[0]; delete [] work; work=new double[lwork];
   liwork=iwork[0]; delete [] iwork; iwork=new int[liwork];

   F77_FUNC(dsyevr,DSYEVR)("N", "I", "U", &n, da, &n, &vl, &vu, &one, &n ,
                            &abstol, &m, evals, evecs, &n,
                            isup, work, &lwork, iwork, &liwork, &info);
   if (info!=0) return info;

   // Transfer the eigenvalues and eigenvectors to the output 
   ldet=0; for(unsigned i=0;i<M.cl;i++){ ldet+=log(evals[i]); }

   // Deallocate all the memory used by the various arrays
   delete[] da; delete [] work; delete [] evals; delete[] evecs; delete [] iwork; delete [] isup;

   return 0;
}

}
#endif