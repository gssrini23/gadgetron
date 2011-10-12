#pragma once

#include "matrixOperator.h"
#include "vector_td_utilities.h"

#include <boost/smart_ptr.hpp>
#include <vector>

template <class REAL, class ARRAY_TYPE_REAL, class ARRAY_TYPE_OPERATOR> class imageOperator : public matrixOperator<REAL, ARRAY_TYPE_OPERATOR>
{
  
public:
  
  imageOperator() : matrixOperator<REAL,ARRAY_TYPE_OPERATOR>() {}
  virtual ~imageOperator() {}
  
  // Get regularization image
  inline ARRAY_TYPE_REAL* get() { return image_.get(); }
    
  virtual void operator_scal( REAL, ARRAY_TYPE_OPERATOR* ) = 0;
  virtual void operator_reciprocal( ARRAY_TYPE_REAL* ) = 0;
  virtual REAL operator_asum( ARRAY_TYPE_OPERATOR* ) = 0;
  virtual boost::shared_ptr< ARRAY_TYPE_REAL > operator_norm_squared( ARRAY_TYPE_OPERATOR* ) = 0;
  virtual bool operator_clear( ARRAY_TYPE_OPERATOR* ) = 0;
  virtual bool operator_axpy( ARRAY_TYPE_REAL*, ARRAY_TYPE_OPERATOR*, ARRAY_TYPE_OPERATOR* ) = 0;

  // Compute regularization image (apply the adjoint encoding operator on the encoded image)
  virtual int compute( ARRAY_TYPE_OPERATOR *image )
  {     
    // Make temporary copy of input
    ARRAY_TYPE_OPERATOR tmp(*image); 

    // Normalize to an average energy of "one intensity unit per image element"
    REAL sum = operator_asum( &tmp );
    REAL scale = ( (REAL) tmp.get_number_of_elements()/sum );
    operator_scal( scale, &tmp );
    
    // Square and reciprocalize image
    image_ = operator_norm_squared(&tmp);
    operator_reciprocal(image_.get());
    
    return 0;
  }
  
  // Apply regularization image operator
  virtual int mult_MH_M( ARRAY_TYPE_OPERATOR *in, ARRAY_TYPE_OPERATOR *out, bool accumulate = false )
  {    
    bool ret2 = true;
    
    if( !accumulate ) 
      ret2 = operator_clear( out );
    
    if( ret2 )
      ret2 = operator_axpy( image_.get(), in, out );
    else 
      ret2 = false;
        
    if( ret2 )
      return 0;
    else{
      std::cout << std::endl << "imageOperator::mult_MH_M failed" << std::endl;
      return -1;
    }
  }
  
  virtual int mult_M( ARRAY_TYPE_OPERATOR *in, ARRAY_TYPE_OPERATOR *out, bool accumulate = false ){
    std::cout << std::endl << "imageOperator::mult_M not defined." << std::endl;
    return -1;
  }
  
  virtual int mult_MH( ARRAY_TYPE_OPERATOR *in, ARRAY_TYPE_OPERATOR *out, bool accumulate = false ){
    std::cout << std::endl << "imageOperator::mult_MH not defined." << std::endl;
    return -1;
  }
  
protected:
  boost::shared_ptr< ARRAY_TYPE_REAL > image_;
};