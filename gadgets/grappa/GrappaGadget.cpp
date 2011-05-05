#include "Gadgetron.h"
#include "GrappaGadget.h"
#include "GadgetXml.h"
#include "FFT.h"

GrappaGadget::GrappaGadget()
{

}
 
GrappaGadget::~GrappaGadget()
{
  for (unsigned int i = 0; i < buffers_.size(); i++) {
    if (buffers_[i]) delete buffers_[i];
    buffers_[i] = 0;
  }

}

int GrappaGadget::process_config(ACE_Message_Block* mb)
{
  TiXmlDocument doc;
  doc.Parse(mb->rd_ptr());

  dimensions_.push_back(GetIntParameterValueFromXML(&doc, "encoding", "readout_length"));
  dimensions_.push_back(GetIntParameterValueFromXML(&doc, "encoding", "matrix_y"));
  dimensions_.push_back(GetIntParameterValueFromXML(&doc, "encoding", "matrix_z"));
  dimensions_.push_back(GetIntParameterValueFromXML(&doc, "encoding", "channels"));
  dimensions_.push_back(GetIntParameterValueFromXML(&doc, "encoding", "slices"));

  image_dimensions_.push_back(dimensions_[0]); //This should be changed when we deal with oversampling
  image_dimensions_.push_back(dimensions_[1]);
  image_dimensions_.push_back(dimensions_[2]);
  image_dimensions_.push_back(dimensions_[3]);


  buffers_ = std::vector<GrappaCalibrationBuffer* >(dimensions_[4],0);
  for (unsigned int i = 0; i < buffers_.size(); i++) {
    buffers_[i] = new GrappaCalibrationBuffer(dimensions_);
  }

  image_data_ = std::vector< GadgetContainerMessage< hoNDArray< std::complex<float> > >* >(dimensions_[4],0);
  for (unsigned int i = 0; i < image_data_.size(); i++) {
    if (create_image_buffer(i) != GADGET_OK) {
      GADGET_DEBUG1("Unable to create image buffers");
      return GADGET_FAIL;
    }
  }

  return GADGET_OK;
}

int GrappaGadget::
process(GadgetContainerMessage<GadgetMessageAcquisition>* m1,
	GadgetContainerMessage< hoNDArray< std::complex<float> > >* m2)
{

  GadgetMessageAcquisition* acq_head = m1->getObjectPtr();
  
  unsigned int samples =  acq_head->samples;
  unsigned int line = acq_head->idx.line;
  unsigned int partition = acq_head->idx.partition;
  unsigned int slice = acq_head->idx.slice;

  //TODO:
  //2. If last line in slice, FFT and apply weights
  //3. Hand off data to GrappaBuffers //We will do this last to not delay pushing out images
  
  if (samples != image_dimensions_[0]) {
    GADGET_DEBUG1("GrappaGadget: wrong number of samples received\n");
    m1->release();
    return GADGET_FAIL;    
  }

  if (slice >= image_data_.size()) {
    GADGET_DEBUG1("Invalid slice number received\n");
    m1->release();
    return GADGET_FAIL;
  }

  if (!image_data_[0]) {
    if (create_image_buffer(slice) != GADGET_OK) {
      GADGET_DEBUG1("Failed to allocate new slice buffer\n");
      return GADGET_FAIL;
    }
  }

  std::complex<float>* b = image_data_[slice]->getObjectPtr()->get_data_ptr();
  std::complex<float>* d = m2->getObjectPtr()->get_data_ptr();

  size_t offset= 0;
  //Copy the data for all the channels
  for (int c = 0; c < m1->getObjectPtr()->channels; c++) {
    offset = 
      c*image_dimensions_[0]*image_dimensions_[1]*image_dimensions_[2] +
      partition*image_dimensions_[0]*image_dimensions_[1] +
      line*image_dimensions_[0];
    
    memcpy(b+offset,d+c*samples,sizeof(std::complex<float>)*samples);
  }


  bool is_last_scan_in_slice =
    (m1->getObjectPtr()->flags & GADGET_FLAG_LAST_ACQ_IN_SLICE);
  
  if (is_last_scan_in_slice) {
    FFT<float> ft;

    GadgetContainerMessage<GadgetMessageImage>* cm1 = 
      new GadgetContainerMessage<GadgetMessageImage>();
    
    cm1->cont(image_data_[slice]);
    

    cm1->getObjectPtr()->matrix_size[0] = image_dimensions_[0];
    cm1->getObjectPtr()->matrix_size[1] = image_dimensions_[1];
    cm1->getObjectPtr()->matrix_size[2] = image_dimensions_[2];
    cm1->getObjectPtr()->channels       = image_dimensions_[3];
    cm1->getObjectPtr()->data_idx_min       = m1->getObjectPtr()->min_idx;
    cm1->getObjectPtr()->data_idx_max       = m1->getObjectPtr()->max_idx;
    cm1->getObjectPtr()->data_idx_current   = m1->getObjectPtr()->idx;	

    memcpy(cm1->getObjectPtr()->position,m1->getObjectPtr()->position,
	   sizeof(float)*3);

    memcpy(cm1->getObjectPtr()->quarternion,m1->getObjectPtr()->quarternion,
	   sizeof(float)*4);


    
    //TODO:
    //apply weights
    if (!image_data_[slice]) {
      GADGET_DEBUG1("Image buffer point is zero, not expected\n");
      return GADGET_FAIL;
    }


    if (!image_data_[slice]->getObjectPtr()) {
      GADGET_DEBUG1("Image buffer point is zero, not expected\n");
      return GADGET_FAIL;
    }

    ft.ifft(image_data_[slice]->getObjectPtr(),0);
    //ft.ifft(image_data_[slice]->getObjectPtr(),1);
    //ft.ifft(image_data_[slice]->getObjectPtr(),2);

    if (this->next()->putq(cm1) < 0) {
      GADGET_DEBUG1("Failed to pass image on to next Gadget in chain\n");
      return GADGET_FAIL;
    }

    //For allocation of new image buffer next time around.
    image_data_[slice] = 0;

  }

  m1->release();
  return GADGET_OK;
}


int GrappaGadget::create_image_buffer(unsigned int slice)
{
  if (slice >= image_data_.size()) {
    return GADGET_FAIL;
  }

  if (image_data_[slice] != 0) {
    image_data_[slice]->release();
    image_data_[slice] = 0;
  }

  image_data_[slice] = new GadgetContainerMessage< hoNDArray< std::complex<float> > >();
  if (!image_data_[slice]->getObjectPtr()->create(image_dimensions_)) {
    GADGET_DEBUG1("Unable to create image buffers");
    return GADGET_FAIL;
  } 

  std::fill(image_data_[slice]->getObjectPtr()->get_data_ptr(),
	    image_data_[slice]->getObjectPtr()->get_data_ptr()+image_data_[slice]->getObjectPtr()->get_number_of_elements(),
	    std::complex<float>(0.0f,0.0f));

  return GADGET_OK;

}

GADGET_FACTORY_DECLARE(GrappaGadget)
