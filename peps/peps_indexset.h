
#ifndef _PEPS_INDEXSET_H_
#define _PEPS_INDEXSET_H_

#include "lattice.h"

//
//PEPSt_IndexSet_Base class
//the lower t is for "template"
//
//restore the indices of physical ones and virtual ones for certain lattice
//
//
template <class IndexT>
class PEPSt_IndexSet_Base
{
    public:
        PEPSt_IndexSet_Base(const int &d, const int &D, const int &n_sites_total, const int &n_bonds_total);

        //
        //Access methods
        //
        inline const int &d() const
        {
            return d_;
        }

        inline const int &D() const
        {
            return D_;
        }
        
        inline const IndexT &phys_legs(const int &site_i) const
        {
            return phys_legs_[site_i];
        }

        inline const IndexT &virt_legs(const int &bond_i) const
        {
            return virt_legs_[bond_i];
        }

    protected:
        //dimension of single physical/virtual leg.
        int d_, D_;
        std::vector<IndexT> phys_legs_, virt_legs_;
};

//
//Indices for general PEPS
//using Index
//
class PEPS_IndexSet : public PEPSt_IndexSet_Base<Index>
{
    public:
        //
        //Constructor
        //
        PEPS_IndexSet(const int &d, const int &D, const Lattice_Torus_Base &lattice);

        //
        //Constructor Helper
        //
        void init_phys_legs(const int &n_sites_total);
        void init_virt_legs(const int &n_bonds_total);
};

//
//Indices for spin one half PEPS
//using IQIndex
//
class IQPEPS_IndexSet_SpinHalf : public PEPSt_IndexSet_Base<IQIndex>
{
    public:
        //
        //Constructor
        //
        //D=sum_{i=0}^{n}{2*i/2+1}. Namely, a virtual leg is formed by
        //0\circplus 1/2\circplus 1 ...
        IQPEPS_IndexSet_SpinHalf(const int &D, const Lattice_Torus_Base &lattice);
        //specify the quantum number in virt_leg_spin: 
        //virt_leg_spin[i] = # of spin i/2
        //should be translated to Sz quantum number
        IQPEPS_IndexSet_SpinHalf(const int &D, const std::vector<int> &virt_leg_spin, const Lattice_Torus_Base &lattice);

        //
        //Constructor Helpers
        //Initialize physical/virtual legs
        //
        void init_phys_legs(const int &n_sites_total);
        void init_virt_legs(const int &n_bonds_total, const int &spin_dim, const std::vector<int> &bond_indqn_deg);

    private:

};

#endif