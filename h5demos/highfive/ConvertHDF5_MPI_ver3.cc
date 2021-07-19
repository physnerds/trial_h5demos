#ifndef H5_WRITE_CHARS_H
#define H5_WRITE_CHARS_H

#include <highfive/H5DataSet.hpp>
#include <highfive/H5DataSpace.hpp>
#include <highfive/H5DataType.hpp>
#include <highfive/H5File.hpp>


#include "h5_write_1D_chars.h"
#include "h5_write_1D_chars_MPI.h"
#include "serialize_dataprods.h"
#include "utilities.h"


//#include "dkmeta.h"
#include "dk2nu.h"

#include <string>
#include <cstddef>
#include <vector>
#include <iostream>
#include <stdlib.h>
#include <iomanip>
#include "TH1D.h"
#include "TChain.h"
#include "TH2D.h"
#include "TFile.h"
#include "hdf5.h"
#include "h5_setup_mpi.h"
#include "mpi.h"
#include "H5FDmpio.h"
//#include "H5Cpp.h"
using namespace HighFive;
using product_t = std::vector<char>;

#define RANK 2 //This is not MPI RANK
#define NAME_LENGTH 17
#define DATASETNAME "CCQENu"
//#define GROUPNAME "lumi"
#define NX 8
#define NY 4
#define CH_NX 2
#define CH_NY 2


void ConvertHDF5_MPI(char* input_file_dir,int batch,int run_num,int lumi_num,std::string const& outname ){
  int argc; char **argv;
  int flag;
  //just the boiler plate from the example......
  //need to understand what each of these variables are needed for.....
  
  hid_t file_id;      //File ID
  hid_t filespace,memspace;
  //hid_t plist_id;
  int i;
  herr_t status;
  hid_t fapl_id;     //File access property list
  hid_t run,lumi;   //Group ID
  hid_t dset_id;    // DataSet ID
  hid_t chunk_id;   //Chunk ID
  hid_t file_space_id;     // File DataSpace ID
  hsize_t file_dims[RANK];
  hsize_t max_dims[RANK];
  hsize_t chunk_dims[RANK];
  hsize_t dimsf[2];
  herr_t ret;     // Generic return value (whatever it means...)
  std::vector<product_t>products;
  char groupname[16];
  std::string ntuple_name = "ccqe_data";


  
  MPI_Initialized(&flag);
  if(!flag)MPI_Init(&argc,&argv);
  //also put the global variables....

  int global_size,global_rank;
  
  MPI_Comm_size(MPI_COMM_WORLD,&global_size);
  MPI_Comm_rank(MPI_COMM_WORLD,&global_rank);

  MPI_Info info;
  MPI_Info_create(&info);
  
  std::cout<<"MPI RANK "<<global_rank<<" MPI SIZE "<<global_size<<std::endl;
  //exit(1);
  if(IsMotherRank()){
    std::cout<<"This is the Mother "<<std::endl;

  }

  //lets try to put the barrier and see if I can get rid of the error::
  // MPI_Barrier(MPI_COMM_WORLD);
  //now lets try to setup the file access property list with parallel I/O access
  auto plist_id = H5Pcreate (H5P_FILE_ACCESS);
  //make sure that we have created the file access property list...
  assert(plist_id>0);
  std::cout<<"File access property id is "<<fapl_id<<std::endl;
  //Below function only works if HDF5-Parallel is implemented.
    ret = H5Pset_fapl_mpio(plist_id,MPI_COMM_WORLD,info);
    assert(ret>=0);
    
    auto H5FILE_NAME = outname.c_str();
    file_id = H5Fcreate(H5FILE_NAME, H5F_ACC_TRUNC, H5P_DEFAULT, plist_id);
    auto run_name = "/run";
    auto scalar_id = H5Screate(H5S_SCALAR);
    run = H5Gcreate(file_id,run_name,H5P_DEFAULT,H5P_DEFAULT,H5P_DEFAULT);
    auto scalar_attr = H5Acreate(run,run_name,H5T_NATIVE_INT,scalar_id,H5P_DEFAULT,H5P_DEFAULT);

    auto att_status = H5Awrite(scalar_attr,H5T_NATIVE_INT,&run_num);
    //okay inside group create lumi....
    auto lumi_name = "lumi";
    lumi = H5Gcreate(run,lumi_name,H5P_DEFAULT,H5P_DEFAULT,H5P_DEFAULT);
    //create the attribute for lumi....
    auto lumi_att_id = H5Screate(H5S_SCALAR);
    auto lumi_attr = H5Acreate(lumi,lumi_name,H5T_NATIVE_INT,lumi_att_id,H5P_DEFAULT,H5P_DEFAULT);

    std::cout<<" Am I here Yet "<<std::endl;
    
    auto f = TFile::Open(input_file_dir);
    std::cout<<" Am I here Yet 2"<<std::endl; 
    auto e = (TTree*)f->Get(ntuple_name.c_str());
    auto l = e->GetListOfBranches();
    //get the total number of branches....
     std::cout<<" Am I here Yet 3"<<std::endl;  
    int nbatch = 0;
    int round = 0;
    auto nentries = e->GetEntriesFast();
    auto tot_branches = l->GetEntriesFast();
    auto classes = return_classes(l);
    auto dset_names = return_dsetnames(l);
    std::cout<<"Total Number of Entries are "<<nentries<<" Total Branches "<<tot_branches<<std::endl;
    //  MPI_Barrier(MPI_COMM_WORLD);
      nentries = 100;
    int counter = 0;
    SetTotalBranches(tot_branches);
    //Whole new green field to test here :)
    //need to do it differently....
    int _itot = nentries/global_size;
    std::cout<<_itot<<" AND "<<nentries%global_size<<std::endl;
    if (nentries%global_size!=0)_itot += nentries%global_size;
    for(int i=0;i<_itot;i++){
      int _div = i/global_size;
      int _mod = 0;
      if(_div!=0)_mod = i%global_size;
      // auto j = global_rank+(global_size+_div+_mod)*i;
      auto j = global_rank+global_size*i;
      // if(j>=nentries)break;
      e->GetEntry(j);
      for(Long64_t jentry=0;jentry<tot_branches;++jentry){
	auto b = dynamic_cast<TBranch*>((*l)[jentry]);
	auto dataprod_name = b->GetName();
	if(classes[jentry]==nullptr){
	  auto blob = return_fundamental_blobs(b);
	  products.push_back(blob);

	}
	else{
	  auto blob = return_blob(b,classes[jentry]);
	  products.push_back(blob);

	}	

      }
      nbatch++;
      if(nbatch==batch||j==nentries-1){
	int rank = GetMPILocalRank();
       	write_1D_chars_MPI(products,dset_names,batch,nbatch,round,lumi,global_rank,global_size,j,counter);
 
	nbatch=0;
	products.clear();
	++round;

      }

      //   std::cout<<"HERE "<<global_rank<<" "<<j<<" "<<i<<" "<<_div<<" "<<_mod<<std::endl;
      counter++;

    }
    MPI_Barrier(MPI_COMM_WORLD);
    size_t obj_id = H5Fget_obj_count(file_id,H5F_OBJ_ALL);
    std::cout<<"Total Open Objects are "<<obj_id<<" in rank "<<global_rank<<std::endl;

    obj_id = H5Fget_obj_count(file_id,H5F_OBJ_GROUP);
    std::cout<<"Total Open groups are "<<obj_id<<" in rank "<<global_rank<<std::endl;
    
    obj_id = H5Fget_obj_count(file_id,H5F_OBJ_DATATYPE);
    std::cout<<"Total Open datatypes are "<<obj_id<<" in rank "<<global_rank<<std::endl;

    obj_id = H5Fget_obj_count(file_id,H5F_OBJ_DATASET);
    std::cout<<"Total Open datasets are "<<obj_id<<" in rank "<<global_rank<<std::endl;    
    
    obj_id = H5Fget_obj_count(file_id,H5F_OBJ_ATTR);
    std::cout<<"Total Open attributes are "<<obj_id<<" in rank "<<global_rank<<std::endl;

    obj_id = H5Fget_obj_count(file_id,H5F_OBJ_FILE);
    std::cout<<"Total Open files are "<<obj_id<<" in rank "<<global_rank<<std::endl;    
    
    H5Sclose(scalar_id);
    H5Sclose(lumi_att_id);
    H5Pclose(plist_id);

    H5Gclose(lumi);
    H5Gclose(run);

    //close the attributes...
    H5Aclose(scalar_attr);
    H5Aclose(lumi_attr);

    //in the end close the file.

    //lets check the number of open obj id in case there are any....
    H5Fclose(file_id);
    MPI_Finalize();
}


#endif
int main(int argc, char* argv[]){

  if(argc!=4){
    std::cerr << "Please specify input file, batch-size and outputfilename\n";
    return 1;
}

  auto run_num = 1;
  auto lumi_num = 1;
  auto input_filename = argv[1];
  auto batch_size = atoi(argv[2]);
  auto outname = argv[3];

  int rank,size;
  
  int ierr;
  ierr = MPI_Init(&argc,&argv);
  if(ierr!=0){
    std::cout << "\n";
    std::cout << "BONES - Fatal error!\n";
    std::cout << "  MPI_Init returned ierr = " << ierr << "\n";
    exit ( 1 );
  }

  // MPI_Comm_rank(MPI_COMM_WORLD,&rank);

  ConvertHDF5_MPI(input_filename,batch_size,run_num,lumi_num,outname);
  // MPI_Finalize(); //for some reason, I cannot finalize the mpich routine
  return 1;
  }
