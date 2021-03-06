//*LB*
// Copyright (c) 2010, University of Bonn, Institute for Computer Science VI
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
//  * Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//  * Neither the name of the University of Bonn 
//    nor the names of its contributors may be used to endorse or promote
//    products derived from this software without specific prior written
//    permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//*LE*





#include <string>
#include <iostream>
#include <fstream>
#include <boost/python.hpp>
#include <boost/python/extract.hpp>
#include <boost/python/list.hpp>

#include  <boost/type_traits/is_same.hpp> 

#include <pyublas/numpy.hpp>

#include <cuv/basics/dia_matrix.hpp>
#include <cuv/basics/sparse_matrix_io.hpp>
#include <cuv/basics/filter_factory.hpp>
#include <cuv/matrix_ops/matrix_ops.hpp>
#include <cuv/matrix_ops/densedense_to_sparse.hpp>
#include <cuv/convert/convert.hpp>

namespace ublas = boost::numeric::ublas;
using namespace boost::python;
using namespace cuv;

template<class T>
boost::shared_ptr<T>
create_dia_mat(unsigned int h, unsigned int w, boost::python::list& dia_offsets, unsigned int stride, unsigned int rf=1){
	int num_dia=boost::python::len(dia_offsets);
	int *dias = new int[num_dia];
	for(int i=0; i< num_dia; i++) {
		int ls = boost::python::extract<int>(dia_offsets[i]);
		dias[i]=ls;
	}
	boost::shared_ptr<T> m ( new T(h,w,num_dia,stride,rf));
	m->set_offsets(dias,dias+num_dia);
	delete[] dias;
	return m;
}

template<class T>
boost::shared_ptr<T>
create_dia_mat_empty(){
	boost::shared_ptr<T> m(new T());
	return m;
}
template<class T>
boost::shared_ptr<T>
create_dia_mat_from_dia_mat(T* other){
	int *dias = new int[other->num_dia()];
	for(int i=0; i< other->num_dia(); i++) {
		dias[i]=other->get_offset(i);
	}
	boost::shared_ptr<T> m(new T(other->h(),other->w(),other->num_dia(),other->stride(),other->row_fact()));
	m->set_offsets(dias,dias+other->num_dia());
	delete[] dias;
	return m;
}
template<class V, class I>
struct dia_io{
	static void save_dia_mat(dia_matrix<V,host_memory_space,I>& m, std::string fn){
			std::ofstream ofs(fn.c_str());
			boost::archive::binary_oarchive oa(ofs);
			oa << m;
	}
	static void load_dia_mat(dia_matrix<V,host_memory_space,I>& m, std::string fn){
			std::ifstream ifs(fn.c_str());
			boost::archive::binary_iarchive ia(ifs);
			ia >> m;
	}
	static void save_dia_mat(dia_matrix<V,dev_memory_space,I>& m, std::string fn){
		dia_matrix<V,host_memory_space,I> m2(m.h(),m.w(),m.num_dia(),m.stride(),m.row_fact());
		convert(m2,m);
		save_dia_mat(m2,fn);
	}
	static void load_dia_mat(dia_matrix<V,dev_memory_space,I>& m, std::string fn){
		dia_matrix<V,host_memory_space,I> m2;
		load_dia_mat(m2,fn);
		convert(m,m2);
	}
};

template<class T>
void
export_diamat_common(const char* name){
	typedef T mat;
	typedef typename mat::value_type value_type;
	typedef typename mat::index_type index_type;
	typedef typename mat::vec_type vec_type;

	class_<mat,boost::shared_ptr<mat> > matobj(name);
	matobj
		//.def("w",   &mat::w,    "width")
		//.def("h",   &mat::h,    "height")
		//.def("vec",    (vec_type* (mat::*)())(&mat::vec_ptr), "internal memory vector", return_internal_reference<>())
		.add_property("h", &mat::h)
		.add_property("w", &mat::w)
		.add_property("vec", make_function((vec_type* (mat::*)())(&mat::vec_ptr), return_internal_reference<>()))
		.add_property("stride",&mat::stride, "matrix stride")
		.add_property("num_dia",&mat::num_dia, "number of diagonals")
		.def("__len__",&mat::n, "number of elements")
		.def("alloc",&mat::alloc, "allocate memory")
		.def("dia",(vec_type* (mat::*)(const int&))(&mat::get_dia), "return a view on one of the diagonals", return_value_policy<manage_new_object>())
		.def("dealloc",&mat::dealloc, "deallocate memory")
		.def("save", (void (*)(mat&,std::string)) dia_io<value_type, index_type>::save_dia_mat, "save to file")
		.def("load", (void (*)(mat&,std::string)) dia_io<value_type, index_type>::load_dia_mat, "load from file")
		.def("set", &mat::set, "set a value")
		.def("has", &mat::has, "whether dia matrix stores this value")
		.def("__call__",  (value_type (mat::*)(const typename mat::index_type&, const typename mat::index_type&)const)(&mat::operator())) // igitt.
		.def("__init__",  make_constructor(create_dia_mat<mat>))
		.def("__init__",  make_constructor(create_dia_mat_empty<mat>))
		.def("__init__",  make_constructor(create_dia_mat_from_dia_mat<mat>) )
		;


	//def((std::string("make_")+name).c_str(),  create_dia_mat<mat>,              (arg("h"),arg("w"),arg("offsets"),arg("stride"),arg("steepness")=1), return_value_policy<manage_new_object>());
	//def((std::string("make_")+name).c_str(),  create_dia_mat_from_dia_mat<mat>, return_value_policy<manage_new_object>());
}

template<class T>
void export_block_descriptors(const char*name){
	typedef host_block_descriptor<T> hbd;
	class_<hbd>(
			(std::string("host_block_descriptor_") + name).c_str(), init<const dia_matrix<T,host_memory_space>& >());
	class_<dev_block_descriptor<T>  >(
			(std::string("dev_block_descriptor_") + name).c_str(),  init<const dia_matrix<T,dev_memory_space>&  >())
		.def("__len__", &dev_block_descriptor<T>::len)
		;
}

template<class T>
tensor<T,host_memory_space,column_major> *
dev_dia_mat2tensor(dia_matrix<T,dev_memory_space>&m){
	dia_matrix<T,host_memory_space>   hostdia(m.h(),m.w(),m.num_dia(),m.stride(),m.row_fact());
	cuv::convert(hostdia,m);
	tensor<T,host_memory_space,column_major>* mdense= new tensor<T,host_memory_space,column_major>(m.shape());
	cuv::convert(*mdense,hostdia);
	//pyublas::numpy_matrix<T,ublas::column_major> to = host_dense_mat2numpy<T,cuv::column_major,ublas::column_major,cuv::column_major>(mdense);
	return mdense;
}


template<class M>
void
export_filter_factory(const char* name){
	typedef M mat;
	typedef typename mat::value_type value_type;
	typedef typename mat::memory_space memory_space;
	typedef typename mat::index_type index_type;

	class_<filter_factory<value_type, memory_space> > (name, init<int, int, int, int, int>())
		.def("extract_filter",(tensor<value_type, memory_space, row_major>*  (filter_factory<value_type, memory_space>::*)(const dia_matrix<value_type, host_memory_space>&, unsigned int))
				&filter_factory<value_type, memory_space>::extract_filter, (
					arg("dia matrix"), arg("filter number")),
				return_value_policy<manage_new_object>())
		.def("extract_filter",(tensor<value_type, memory_space, row_major>*  (filter_factory<value_type, memory_space>::*)(const dia_matrix<value_type, dev_memory_space>&, unsigned int))
				&filter_factory<value_type, memory_space>::extract_filter, (
					arg("dia matrix"), arg("filter number")),
				return_value_policy<manage_new_object>())
		//.def("extract_filters",(dense_matrix<value_type, column_major, memory_space>*  (*)(const toeplitz_matrix<value_type, memory_space>&))
		//                                    &filter_factory<value_type, memory_space>::extract_filters, (
		//                                                            arg("toeplitz matrix"))
		//                                                            )

		.def("get_dia",(dia_matrix<value_type, memory_space, index_type>*  (filter_factory<value_type, memory_space>::*)())
											&filter_factory<value_type, memory_space>::get_dia,
													"get filter as diagonal matrix",
											return_value_policy<manage_new_object>()
												)
		;
}

template <class T>
void
export_diamat_conversion(){
	def("convert", (void(*)(dia_matrix<T,dev_memory_space>&,const dia_matrix<T,host_memory_space>&)) cuv::convert);
	def("convert", (void(*)(dia_matrix<T,host_memory_space>&,const dia_matrix<T,dev_memory_space>&)) cuv::convert);
	def("convert", (void(*)(tensor<T,host_memory_space,column_major>&, const dia_matrix<T,host_memory_space>&)) cuv::convert);
	def("pull",    dev_dia_mat2tensor<T>,return_value_policy<manage_new_object>());
}

void export_dia_matrix(){
	export_diamat_common<dia_matrix<float,dev_memory_space> >("dev_dia_matrix_f");
	export_diamat_common<dia_matrix<float,host_memory_space> >("host_dia_matrix_f");
	export_block_descriptors<float>("f");
	export_diamat_conversion<float>();
	export_filter_factory<filter_factory<float,host_memory_space> >("filter_factory_float");

	//def("densedense_to_dia", densedense_to_dia<dia_matrix<float,dev_memory_space>, dev_block_descriptor<float>, dev_dense_matrix<float,column_major> >, "C <- A*B', where C is sparse");
	//def("densedense_to_dia", densedense_to_dia<dia_matrix<float,host_memory_space>,host_block_descriptor<float>,host_dense_matrix<float,column_major> >, "C <- A*B', where C is sparse");

	//def("densedense_to_dia", 
			//densedense_to_dia<float,column_major, unsigned int>,
			//(arg("C"),arg("Cbd"),arg("A"),arg("B"),arg("factAB")=1.f,arg("factC")=0.f));
			//"C <- A*B', where C is sparse");
	//def("densedense_to_dia", 
			//densedense_to_dia<float,host_memory_space,column_major,unsigned int>, 
			//(arg("C"),arg("Cbd"),arg("A"),arg("B"),arg("factAB")=1.f,arg("factC")=0.f));
			////"C <- A*B', where C is sparse");

	def("prod", (void (*)(tensor<float,host_memory_space,column_major>&,const dia_matrix<float,host_memory_space>&,const tensor<float,host_memory_space,column_major>&,char, char, const float&, const float& ))cuv::prod<float,host_memory_space,column_major>, 
			(arg("C"),arg("A"),arg("B"),arg("transA"),arg("transB"),arg("factAB")=1.f,arg("factC")=0.f));
	def("prod", (void (*)(tensor<float,dev_memory_space,column_major>&,const dia_matrix<float,dev_memory_space>&,const tensor<float,dev_memory_space,column_major>&,char, char, const float&, const float& ))cuv::prod<float,dev_memory_space,column_major>,
			(arg("C"),arg("A"),arg("B"),arg("transA"),arg("transB"),arg("factAB")=1.f,arg("factC")=0.f));
}
