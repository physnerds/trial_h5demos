#ifndef H5_WRITE_1D_CHARS_MPI_H
#define H5_WRITE_1D_CHARS_MPI_H

#include "utilities.h"
#include "serialize_dataprods.h"
//#include "mpi.h"
//#include "h5_setup_mpi.h

//eventually need to get rid of highfive...
using namespace HighFive;
using product_t = std::vector<char>;
/*
template<typename T>
void CreateOffsetDataSet(Group lumi,T val, unsigned long batch,std::string offset_name){
  DataSetCreateProps props;
  props.add(Chunking(std::vector<hsize_t>{batch}));
  DataSpace dataspace = DataSpace({batch},{DataSpace::UNLIMITED});
  DataSet dataset = lumi.createDataSet<int>(offset_name,dataspace,props);
  dataset.write(val);
}
*/

struct dataset_info{
  // Things I would like to save from the data space easily
  std::string name; //name of the dataset
  hid_t dset_id; //identifier of the data-set
  hid_t dspace_id; //identifier of the data-space
  hid_t dprop_id; //identifier of the property of the data-set
  hid_t memspace_id;
  hid_t plist_id;

};

//these variables to save info....
std::map<std::string,dataset_info>list_createInfo;
std::map<std::string, dataset_info>size_createInfo;
int __tot_branches=0;
bool Writing_Data=false;
//end of these variables



//need to write the value and return the dataset_id
template<typename T>
hid_t CreateOffSetDataSetMPI(hid_t group_id/* Supposed to be lumi ID*/, T val, unsigned long batch, std::string offset_name, hsize_t* f_dims, hsize_t* max_dims){ //For now MAX DIMS BETTER BE SET AT H5S_UNLIMITED 
  //create the offsetdata-space...
  auto d_space = H5Screate_simple(0,f_dims, max_dims); 
  //create the attribute for the data space
  auto d_attid = H5Screate(H5S_SIMPLE); //H5S_SIMPLE -->REGULAR ARRAY OF ELEMENTS.
  auto plist_id = H5Pcreate(H5P_DATASET_CREATE);
  hsize_t chunk_dim[1] = {batch};
  H5Pset_chunk(plist_id,1,chunk_dim); 
  auto d_id = H5Dcreate(group_id, offset_name.c_str(),H5T_NATIVE_INT,
			d_space,H5P_DEFAULT,plist_id,H5P_DEFAULT);
  //now write the info...
  h5Dwrite(d_id,H5T_NATIVE_INT,H5T_NATIVE_INT,H5S_ALL,H5S_ALL,
	   H5P_DEFAULT,&val);
  return d_id;
}

std::vector<char>
flatten_MPI(std::vector<std::vector<char>> const& tmp) {
  std::vector<char> tmp1 = tmp[0];
  for (int i = 1; i<tmp.size(); ++i)
     tmp1.insert(end(tmp1), begin(tmp[i]), end(tmp[i]));
  return tmp1;
}


//now write into the data-sets....
//should be able to extend the data-set and write into it.

//now we write the metadata...where we store what the size of each event buffer is....
hid_t WriteMetaData(std::string branch_name,char *buff_size, hid_t lumi_id){
  auto plist_id = (size_createInfo[branch_name]).plist_id;
  auto size_name = branch_name+"_size";
  auto dataset_id = H5Dopen(lumi_id,size_name.c_str(),plist_id);
  auto dspace_id = H5Dget_space(dataset_id);
  auto ndims = H5Sget_simple_extent_ndims(dspace_id);
  hsize_t num_blocks[1] = {1};
  //get the old dimension....
  hsize_t curr_dims[ndims];
  H5Sget_simple_extent_dims(dspace_id,curr_dims,NULL);
  //the size data-set will always have 1 data at a time....
  hsize_t new_dims[1] = {curr_dims[0]+1};

  //now extend the data-set to the new dimension...
  auto status_id = H5Dset_extent(dataset_id,new_dims);

  //get the hyperslab...
  auto space_status = H5Sselect_hyperslab(dspace_id,H5S_SELECT_SET,curr_dims,NULL,
					  num_blocks,new_dims);


  //new memory space based on the new buffer.....
  auto mspace_id = H5Screate_simple(1,new_dims,NULL);

  auto status = H5Dwrite(dataset_id,H5T_NATIVE_CHAR,mspace_id,dspace_id,H5P_DEFAULT,buff_size);

  //make sure you write the data....otherwise no need to continue forward....
  assert(status>=0);

  return dataset_id;
}


hid_t WriteData(std::string branch_name, std::vector<char>buff, hid_t lumi_id,int _mpi_rank,int _mpi_size,int ievt,int x){
  //maybe I need to send an empty buffer
  if(ievt!=(_mpi_rank+x*_mpi_size)){
    //   std::cout<<"Process not  handled "<<ievt<<" "<<x<<" "<<_mpi_rank<<std::endl;
    return 0;
  }
   else{
     std::cout<<"Process handled "<<ievt<<" "<<x<<" "<<_mpi_rank<<std::endl;
   }
  
  //auto plist_id = (list_createInfo[branch_name]).plist_id;
  auto mpi_size = static_cast<hsize_t>(_mpi_size);
  auto mpi_rank = static_cast<hsize_t>(_mpi_rank);
  auto curr_size = static_cast<unsigned long long>(buff.size());
  auto dataset_id = H5Dopen(lumi_id,branch_name.c_str(),H5P_DEFAULT);
  auto dspace_id = H5Dget_space(dataset_id);
  auto ndims =  H5Sget_simple_extent_ndims(dspace_id);
  hsize_t num_blocks[2] = {1,1};
  //now the array to get the old dimension...
  hsize_t curr_dims[ndims];
  H5Sget_simple_extent_dims(dspace_id, curr_dims, NULL);

  //now extended dataspace to accomodate the new data....
  auto buff_size = static_cast<unsigned long long>(buff.size());
  hsize_t new_dims[2] = {curr_dims[0],buff_size+curr_dims[1]};
  //hoping this is what we want...
  hsize_t offset[2] = {mpi_rank, curr_dims[1]};//{buff_size};
  //  std::cout<<" curr_size "<<buff_size<<" "<<new_dims[0]<<" "<<ndims<<" "<<mpi_rank<<std::endl;

  
  //first extend the dataset...
  auto status_id = H5Dset_extent(dataset_id,new_dims);
  //apparently after extending the space, we need to get the space again...
  //https://www.mail-archive.com/hdf-forum@hdfgroup.org/msg01283.html
  
  dspace_id = H5Dget_space(dataset_id);
  hsize_t count[2] = {1,buff_size};
  hsize_t stride[2] = {1,1};
  //get the datas-space now...  
  auto space_status = H5Sselect_hyperslab(dspace_id, H5S_SELECT_SET,offset ,stride,
					  count,num_blocks);
  //new memory space based on the new buffer....
  auto mspace_id = H5Screate_simple(2,count,NULL);

  //now write the buffer into the data....
  char* _buff = buff.data();
  auto status = H5Dwrite(dataset_id,H5T_NATIVE_CHAR,mspace_id,dspace_id,H5P_DEFAULT,_buff);
  //make sure that you have written the data...otherwise no use to go forward...
  assert(status>=0);
  //make sure we write the meta-data as well....
  char _buff_size[1] = {char(buff.size())};
  //  auto meta_status = WriteMetaData(branch_name,_buff_size,lumi_id);

  H5Sclose(mspace_id);
  H5Sclose(dspace_id);
  H5Dclose(dataset_id);
  
  return dataset_id;
}


dataset_info CreateMetaDataSets(std::string branch_name,char* buff_size, hid_t lumi_id){
  hsize_t dimsf[1],//current dimension of the dataset
    max_dims[1], //maximum allowed dimension of the dataset
    mem_dims[1], // size of the memory dimension.
    num_blocks[1], //number of blocks to be selected in the hyperslab.
    hslab_start_pos[1]; // start position of the hyperslab...this is 0 when creating.

    dimsf[0] = 1; 
    max_dims[0] = H5S_UNLIMITED;
    num_blocks[0]=1;
    hslab_start_pos[0] = 0;
    mem_dims[0] = 1; //the size info is just the integer type

    auto dspace_id = H5Screate_simple(1, dimsf,max_dims);
    auto plist_id = H5Pcreate(H5P_DATASET_CREATE);

    H5Pset_chunk(plist_id,1,dimsf);
    auto sz_name = branch_name+"_size";
    auto dset_id = H5Dcreate(lumi_id,sz_name.c_str(),H5T_NATIVE_CHAR,dspace_id,H5P_DEFAULT,
			     plist_id, H5P_DEFAULT);

    auto ret = H5Sselect_hyperslab(dspace_id,H5S_SELECT_SET,hslab_start_pos,
				   NULL,num_blocks,dimsf);
    assert(ret>=0);
    auto mspace_id = H5Screate_simple(1,mem_dims,NULL);
    auto status = H5Dwrite(dset_id,H5T_NATIVE_CHAR,mspace_id,
			   dspace_id,H5P_DEFAULT,buff_size);
    if(status<0)std::cout<<"Something went Wrong Size-DataSets "<<branch_name<<std::endl;

    H5Sclose(mspace_id);
    H5Sclose(dspace_id);
    H5Dclose(dset_id);
    H5Pclose(plist_id);

    dataset_info dinfo;
    dinfo.name = branch_name;
    dinfo.dset_id = dset_id;
    dinfo.dspace_id = dspace_id;
    dinfo.dprop_id = plist_id;
    dinfo.memspace_id = mspace_id;
    dinfo.plist_id = plist_id;

    return dinfo;
}


dataset_info Create2DDataSets(std::string branch_name,std::vector<char>buff,hid_t lumi_id){

  int _mpi_rank,_mpi_size;
  MPI_Comm_size(MPI_COMM_WORLD,&_mpi_size);
  MPI_Comm_rank(MPI_COMM_WORLD,&_mpi_rank);
  
  auto mpi_size = static_cast<hsize_t>(_mpi_size);
  auto mpi_rank = static_cast<hsize_t>(_mpi_rank);

  auto curr_size = static_cast<unsigned long long>(buff.size());

  
  hsize_t dimsf[2] = {mpi_size,curr_size};

  hsize_t max_dims[2] = {mpi_size,H5S_UNLIMITED};

  hsize_t num_blocks[2] = {1,curr_size};

  hsize_t hslab_start_pos[2] = {mpi_rank,0};

  hsize_t mem_dims[2] = {1,curr_size};
  
  
  
  auto dspace_id = H5Screate_simple(2,dimsf,max_dims);
  assert(dspace_id>=0);
  
  auto plist_id = H5Pcreate(H5P_DATASET_CREATE);

  
  H5Pset_chunk(plist_id,2,dimsf);

  auto dset_id = H5Dcreate(lumi_id,branch_name.c_str(),H5T_NATIVE_CHAR,
			   dspace_id,H5P_DEFAULT,plist_id,H5P_DEFAULT);
  auto xf_id = H5Pcreate(H5P_DATASET_XFER);
  H5Pset_dxpl_mpio(xf_id,H5FD_MPIO_COLLECTIVE);

  auto ret = H5Sselect_hyperslab(dspace_id,H5S_SELECT_SET,hslab_start_pos,NULL,num_blocks,NULL);

  assert(ret>=0);

  auto mspace_id = H5Screate_simple(2,mem_dims,NULL);

  assert(mspace_id>=0);

  char *_buff = buff.data();
  auto status = H5Dwrite(dset_id,H5T_NATIVE_CHAR,mspace_id,dspace_id,xf_id,_buff);

  assert(status>=0);

  // char buff_size = {char(buff.size())};
  std::vector<char>buff_size = {char(buff.size())};
  auto size_name = branch_name+"_size";
  
  dataset_info dinfo;

  dinfo.name = branch_name;
  dinfo.dset_id = dset_id;
  dinfo.dspace_id = dspace_id;
  dinfo.dprop_id = xf_id;
  dinfo.memspace_id = mspace_id;
  dinfo.plist_id = xf_id;

  MPI_Barrier(MPI_COMM_WORLD);

  H5Sclose(mspace_id);
  H5Sclose(dspace_id);
  H5Dclose(dset_id);
  H5Pclose(plist_id);
  H5Pclose(xf_id);

  return dinfo;
  
  
}

hid_t Write2DData(std::string branch_name,std::vector<char>buff,hid_t lumi_id){
  int _mpi_rank,_mpi_size;
  MPI_Comm_size(MPI_COMM_WORLD,&_mpi_size);
  MPI_Comm_rank(MPI_COMM_WORLD,&_mpi_rank);
  
  auto curr_size = static_cast<unsigned long long>(buff.size());

  auto mpi_size = static_cast<hsize_t>(_mpi_size);
  auto mpi_rank = static_cast<hsize_t>(_mpi_rank);
  

  auto dataset_id = H5Dopen(lumi_id,branch_name.c_str(),H5P_DEFAULT);

  auto xf_id = H5Pcreate(H5P_DATASET_XFER);

  H5Pset_dxpl_mpio(xf_id,H5FD_MPIO_COLLECTIVE);
  auto dspace_id = H5Dget_space(dataset_id);
    
  auto ndims = H5Sget_simple_extent_ndims(dspace_id);

  hsize_t num_blocks[2] = {1,1};

  hsize_t curr_dims[ndims];
  H5Sget_simple_extent_dims(dspace_id,curr_dims,NULL);
  
  hsize_t new_dims[2] = {curr_dims[0],curr_size+curr_dims[1]};

  hsize_t offset[2] = {mpi_rank,curr_dims[1]};

  auto status_id = H5Dset_extent(dataset_id,new_dims);
  
  //apparently after extending the space, we need to get the space again...
  //https://www.mail-archive.com/hdf-forum@hdfgroup.org/msg01283.html
  
  dspace_id = H5Dget_space(dataset_id);

  hsize_t count[2] = {1,curr_size};
  hsize_t stride[2] = {1,1};
  auto space_status = H5Sselect_hyperslab(dspace_id,H5S_SELECT_SET,offset,stride,
					  count,num_blocks);

  auto mspace_id = H5Screate_simple(2,count,NULL);
  char* _buff = buff.data();

  auto status = H5Dwrite(dataset_id,H5T_NATIVE_CHAR,mspace_id,dspace_id,
			 xf_id,_buff);
  assert(status>=0);
  
  H5Pclose(xf_id);
  H5Sclose(mspace_id);
  H5Sclose(dspace_id);
  H5Dclose(dataset_id);

  return dataset_id;  
}

hid_t Write1DData(std::string branch_name,std::vector<char>buff,hid_t lumi_id, int ievt){
  int mpi_rank,mpi_size;
  MPI_Comm_size(MPI_COMM_WORLD,&mpi_size);
  MPI_Comm_rank(MPI_COMM_WORLD,&mpi_rank);
  
  auto curr_size = static_cast<unsigned long long>(buff.size());
  int prev_nz,tot_nz;

  int tot_size;
  int _curr_size = static_cast<int>(curr_size);
  
  MPI_Scan(&_curr_size,&tot_size,1,MPI_INT,MPI_SUM,MPI_COMM_WORLD);
  
  auto dataset_id = H5Dopen(lumi_id,branch_name.c_str(),H5P_DEFAULT);
  auto xf_id = H5Pcreate(H5P_DATASET_XFER);
  H5Pset_dxpl_mpio(xf_id,H5FD_MPIO_COLLECTIVE);
  auto dspace_id = H5Dget_space(dataset_id);

  auto ndims = H5Sget_simple_extent_ndims(dspace_id);
  hsize_t num_blocks[1] = {1};

  hsize_t curr_dims[ndims];

  H5Sget_simple_extent_dims(dspace_id,curr_dims,NULL);

  int tot_dims;
  int _curr_dims = static_cast<int>(curr_dims[0]);

  //do one more scan to understand this.
  MPI_Scan(&_curr_dims,&tot_dims,1,MPI_INT,MPI_SUM,MPI_COMM_WORLD);

  // hsize_t new_dims[1] = {curr_size+curr_dims[0]+static_cast<hsize_t>(tot_size-curr_size)};
  // hsize_t new_dims[1] = {tot_size+curr_dims[0]};
  // hsize_t new_dims[1] = {curr_size+curr_dims[0]};

  hsize_t new_dims[1] = {curr_dims[0]+curr_size};
  //  if(mpi_rank!=0) new_dims[0] = {tot_size+curr_size};
  
  //    std::cout<<"Current size "<<curr_size<<" Total Size "<<tot_size<<" Current Dimension "<<curr_dims[0]<<" MPI RANK "
  //    	   <<mpi_rank<<" space id "<<dspace_id<<" "<<buff.size()<<std::endl;
    hsize_t adj_dims[1] = {curr_size+curr_dims[0]+static_cast<hsize_t>(tot_size-curr_size)};
  //  hsize_t adj_dims[1] = {curr_size+curr_dims[0]};
  //  if(tot_size!=curr_size)
  //   adj_dims[0] = {static_cast<hsize_t>(tot_size)+curr_dims[0]};
   MPI_Barrier(MPI_COMM_WORLD);
   hsize_t offset[1] = {curr_dims[0]};
   //  if(mpi_rank!=0) offset[0] = {static_cast<hsize_t>(tot_size)};
  //  hsize_t offset[1] = {curr_dims[0]*mpi_rank};
   /*
   if(adj_dims[0]==24)
     std::cout<<"curr dimension "<<_curr_dims<<" total "<<
     tot_dims<<" rank "<<mpi_rank<<std::endl;
   */
   //  hsize_t offset[1] = {curr_dims[0]+static_cast<hsize_t>(tot_size-curr_size)};
   if(adj_dims[0]>1){
     std::cout<<" * "<<mpi_rank<<" | "<<curr_dims[0]<<" | "<<curr_size<<" | "<<tot_size<<
       " | "<<tot_dims<<" || "<<ievt<<" *||* "<<std::endl;
    std::cout<<"***********************************"<<std::endl;
   }
   
  //  hsize_t offset[1] = {curr_dims[0]};
  //maybe i can put an if statement here
  auto status_id = H5Dset_extent(dataset_id,new_dims);

  dspace_id = H5Dget_space(dataset_id);
  hsize_t count[1] = {1};
  //  hsize_t count[1]= {static_cast<hsize_t>(tot_size)};
  hsize_t stride[1] = {1};
  hsize_t buff_size[1] = {curr_size};
  if(tot_size!=curr_size)
    stride[0] = 1;
  H5S_seloper_t op = H5S_SELECT_SET;
  //  if(tot_size!=curr_size)
  //   op = H5S_SELECT_OR;
  auto space_status = H5Sselect_hyperslab(dspace_id,op,
					    offset,NULL,count,buff_size);
					  //  adj_dims,NULL,count,NULL);

  auto mspace_id =  H5Screate_simple(1,buff_size,NULL);
  
  char* _buff = buff.data();
 
   auto status = H5Dwrite(dataset_id,H5T_NATIVE_CHAR,
  			 mspace_id,dspace_id,xf_id,_buff);
   
   assert(status>=0);
   
  char _buff_size[1] = {char(buff.size())};
  H5Pclose(xf_id);
  H5Sclose(mspace_id);
  H5Sclose(dspace_id);
  H5Dclose(dataset_id);
  return dataset_id;

}

dataset_info Create1DDataSets(std::string branch_name,std::vector<char>buff,hid_t lumi_id){

  int mpi_rank,mpi_size;
  MPI_Comm_size(MPI_COMM_WORLD,&mpi_size);
  MPI_Comm_rank(MPI_COMM_WORLD,&mpi_rank);

  auto curr_size = static_cast<unsigned long long>(buff.size());
  hsize_t num_blocks[1],hslab_start_pos[1];
  hsize_t dimsf[1] = {curr_size};
  hsize_t max_dims[1] = {H5S_UNLIMITED};
  num_blocks[0] = curr_size;

  hslab_start_pos[0] = 0;

  hsize_t mem_dims[1]= {curr_size};

  auto dspace_id = H5Screate_simple(1,dimsf,max_dims);

  assert(dspace_id>=0);

  auto plist_id = H5Pcreate(H5P_DATASET_CREATE); /*H5Pcreate(H5P_DATASET_CREATE);*/

  H5Pset_chunk(plist_id,1,dimsf);
  hid_t dset_id;
  dset_id = H5Dcreate(lumi_id,branch_name.c_str(),H5T_NATIVE_CHAR,dspace_id,
			   H5P_DEFAULT,plist_id,H5P_DEFAULT);
  auto xf_id = H5Pcreate(H5P_DATASET_XFER);
  H5Pset_dxpl_mpio(xf_id,H5FD_MPIO_COLLECTIVE);
  
  char* _buff = buff.data();
  auto ret = H5Sselect_hyperslab(dspace_id,H5S_SELECT_SET,hslab_start_pos,
				 NULL,num_blocks,NULL);

  assert(ret>=0);

  auto mspace_id = H5Screate_simple(1,mem_dims,NULL);

  assert(mspace_id>=0);
  
  auto status = H5Dwrite(dset_id,H5T_NATIVE_CHAR,mspace_id,dspace_id,xf_id,_buff);

  assert(status>=0);
    
  char buff_size[1] = {char(buff.size())};

  // auto size_info = CreateMetaDataSets(branch_name,buff_size,lumi_id);

  auto _err = H5Fflush(dset_id,H5F_SCOPE_LOCAL);
  assert(_err>=0);
  MPI_Barrier(MPI_COMM_WORLD);
  dataset_info dinfo;
  dinfo.name = branch_name;
  dinfo.dset_id = dset_id;
  dinfo.dspace_id = dspace_id;
  dinfo.dprop_id = xf_id;
  dinfo.memspace_id = mspace_id;
  dinfo.plist_id = xf_id;

  // MPI_Barrier(MPI_COMM_WORLD);

  H5Sclose(mspace_id);
  H5Sclose(dspace_id);
  H5Dclose(dset_id);
  H5Pclose(plist_id);
  H5Pclose(xf_id);
  
 

  return dinfo;
  
}


dataset_info CreateDataSets(std::string branch_name,std::vector<char>buff,hid_t lumi_id,int _mpi_rank,int _mpi_size){
  //I need to take care of both fundamental and non-fundamental branches....
  //datasets are char type anyway.
  //cast the integer to hsize_t
  auto mpi_size = static_cast<hsize_t>(_mpi_size);
  auto mpi_rank = static_cast<hsize_t>(_mpi_rank);
  auto curr_size = static_cast<unsigned long long>(buff.size());
  //hsize_t dimsf[2],max_dims[2],
  hsize_t num_blocks[2],hslab_start_pos[2];
  hsize_t dimsf[2] = {mpi_size,curr_size};
  hsize_t max_dims[2] = {mpi_size,H5S_UNLIMITED};
  num_blocks[0] = 1; //number of blocks we want is also the buffer size when writing for first time
  num_blocks[1] = curr_size;
  hslab_start_pos[0]=mpi_rank; //when creating, we are writing for the first time....
  hslab_start_pos[1] = 0; 
  //memory dimension is also probably the size of the buffer (always)
  //

  hsize_t mem_dims[2] = {0,curr_size};

  //I think while creating the position is always (0,0)
  hslab_start_pos[0] = mpi_rank;
  hslab_start_pos[1] = 0;
  auto dspace_id = H5Screate_simple(2,dimsf,max_dims);
  assert(dspace_id>=0);
  //auto plist_id = H5Pcreate(H5P_DATASET_XFER);//H5P_DATASET_CREATE-->Regular/ H5P_DATASET_XFER-->Collective
   auto plist_id = H5Pcreate(H5P_DATASET_CREATE);
  //  H5Pset_dxpl_mpio(plist_id,H5FD_MPIO_COLLECTIVE);
  H5Pset_chunk(plist_id, 2,dimsf);

  auto dset_id = H5Dcreate(lumi_id,branch_name.c_str(),H5T_NATIVE_CHAR,dspace_id,
			   H5P_DEFAULT,plist_id,H5P_DEFAULT);

  // auto dset_id = H5Dcreate(lumi_id,branch_name.c_str(),H5T_NATIVE_CHAR,dspace_id,
  //			     H5P_DEFAULT,H5P_DEFAULT,H5P_DEFAULT);

  char* _buff = buff.data();
  // std::cout<<"Size of the buffer "<<buff.size()<<" "<<mpi_rank<<std::endl;
    
  //Select the hyperslab to write
  auto ret = H5Sselect_hyperslab(dspace_id,H5S_SELECT_SET,hslab_start_pos,
				 NULL,num_blocks,NULL);
  assert(ret>=0);
  //H5Pset_chunk(plist_id, 1,dimsf);
  //create a memory dataspace for write buffer..
  auto mspace_id = H5Screate_simple(2,mem_dims,NULL);

  auto status = H5Dwrite(dset_id,H5T_NATIVE_CHAR,mspace_id,dspace_id,H5P_DEFAULT,_buff);
  if(status<0)std::cout<<"Something Went wrong "<<branch_name<<std::endl;
  assert(status>=0);
  //now the status of the CreateMetaData now...
  char buff_size[1] = {char(buff.size())};
  auto size_info = CreateMetaDataSets(branch_name,buff_size,lumi_id);
  size_createInfo[branch_name] = size_info;
  dataset_info dinfo;
  dinfo.name = branch_name;
  dinfo.dset_id = dset_id;
  dinfo.dspace_id = dspace_id;
  dinfo.dprop_id = plist_id;
  dinfo.memspace_id = mspace_id;
  dinfo.plist_id = plist_id;
  //close everything....
  //we will try to close these in once we finish reading and writing....
  
  H5Sclose(mspace_id);
  H5Sclose(dspace_id);
   H5Dclose(dset_id);
  H5Pclose(plist_id);
  std::cout<<"Finished Creating "<<std::endl;
  return dinfo;
  
}



void write_1D_chars_MPI(std::vector<product_t> const& products, 
			std::vector<std::string> const& ds_names, 
			long unsigned int batch, 
			long unsigned int nbatch, 
			long unsigned int round,
			hid_t lumi_id,int mpi_rank,
			int mpi_size, int ievt) 
{
  auto num_prods = ds_names.size();
  for(int prod_index = 0;prod_index<num_prods;++prod_index){
    auto prod_size = products[prod_index].size();
    auto tmp = get_prods(products,prod_index,num_prods);
    auto sizes = get_sizes(tmp);
    auto tmp1 = flatten_MPI(tmp);
    auto sizes1 = get_sizes1D(tmp1);
    auto ds_name = ds_names[prod_index];
    auto sum_prods = std::accumulate(sizes.begin(),sizes.end(),0);
    if(sum_prods==0)continue;
    std::vector<char>sz_vector = {char(tmp1.size())};
    auto szds_name = ds_name+"_sz";
    auto offset_name = ds_name+"_offset";
    // auto dset_id = H5Dopen1(lumi_id,ds_name.c_str());
    //if(!dset_id){
    // if(sum_prods==0)continue; //skip the empty buffers for now....
    //check the number of entries in these branches.
     
    if(list_createInfo.size()!=__tot_branches){
      	auto dset_info =  Create1DDataSets(ds_name,tmp1,lumi_id);
      	list_createInfo[ds_name] = dset_info;
	auto temp = Create1DDataSets(ds_name+"_size",sz_vector,lumi_id);
	std::cout<<"Creating DataSets "<<ievt<<std::endl;

    }

    else{
      // std::cout<<list_createInfo.size()<<" "<<__tot_branches<<std::endl;
      auto status = Write1DData(ds_name,tmp1,lumi_id,ievt);
      	assert(status>=0);
	//std::cout<<"size "<< tmp1.size()<<" "<<std::endl;
		
	//	status = Write1DData(ds_name+"_size",sz_vector,lumi_id,ievt);
	//	assert(status>=0);
    }
    
  }

}




void SetTotalBranches(int nbranch){
  __tot_branches=nbranch;
}


#endif