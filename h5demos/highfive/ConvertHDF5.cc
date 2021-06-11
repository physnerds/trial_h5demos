#ifndef H5_WRITE_CHARS_H
#define H5_WRITE_CHARS_H

#include <highfive/H5DataSet.hpp>
#include <highfive/H5DataSpace.hpp>
#include <highfive/H5DataType.hpp>
#include <highfive/H5File.hpp>

#include "h5_write_1D_chars.h"
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

using namespace HighFive;
using product_t = std::vector<char>;

void ConvertHDF5(char* input_file_dir,int batch,int run_num,int lumi_num,std::string const& outname ){
std::cout<<"At plotdk2nu now"<<std::endl;
int iread=0;

std::vector<product_t>products;
std::string ntuple_name = "CCQENu";
auto f = TFile::Open(input_file_dir);
 auto e = (TTree*)f->Get(ntuple_name.c_str());
 auto l = e->GetListOfBranches();
 std::cout<<"HERE "<<std::endl;
int ievt = 0;
int nbatch=0;
int round = 0;
auto nentries = e->GetEntriesFast();
nentries = 10;
//Create the HDF5 File
File file(outname,File::ReadWrite | File::Create);
//HDF5 Attributes
std::cout<<"Creating HDF5 attributes "<<std::endl;
Group run = file.createGroup("run");
Attribute a = run.createAttribute<int>("run_num",DataSpace::From(run_num));
a.write(run_num);
Group lumi = run.createGroup("lumi");
Attribute b = lumi.createAttribute<int>("lumi_num",DataSpace::From(lumi_num));
b.write(lumi_num);
std::cout<<"Finished Creating and Writing HDF5 Attributes "<<std::endl;
auto ds_names = return_dsetnames(l);
 
auto classes = return_classes(l);
//for(TClass* _class:classes)std::cout<<_class->GetName()<<std::endl;
for(std::string name:ds_names)std::cout<<"DataSet Name "<<name<<std::endl;
while(ievt<nentries){ 
  e->GetEntry(ievt); 
  for (Long64_t jentry=0;jentry<l->GetEntriesFast();++jentry){
       auto b = dynamic_cast<TBranch*>((*l)[jentry]);
       auto dataprod_name = b->GetName();
       if(classes[jentry]==nullptr){
	 // continue;
	 std::cout<<"Name of the branch "<<dataprod_name<<" "<<ievt<<std::endl; 
	 auto blob = return_fundamental_blobs(b);
	 products.push_back(blob);
       }

       else{
	 std::cout<<"Name of the branch "<<dataprod_name<<" "<<ievt<<std::endl; 
	 auto blob = return_blob(b,classes[jentry]);
	 products.push_back(blob);
       }
    }

     nbatch++;
//Need to write into hdf5 if batch number is reached or end of events....
    if(nbatch==batch || ievt == nentries-1){
      std::cout<<"size of the products "<<products.size()<<std::endl;
     write_1D_chars(products,ds_names,batch,nbatch,round,lumi);
     nbatch=0;
     products.clear();
     ++round;
   }

   ++ievt;
  }
  file.flush();
}
#endif
int main(int argc, char* argv[]){

  if(argc!=4){
    std::cerr << "Please specify input file, batch-size and outputfilename\n";
    return 1;
}
  auto input_filename = argv[1];
  auto batch_size = atoi(argv[2]);
  auto run_num = 1;
  auto lumi_num = 1;
  auto outname = argv[3];
  ConvertHDF5(input_filename,batch_size,run_num,lumi_num,outname);
  return 1;
}