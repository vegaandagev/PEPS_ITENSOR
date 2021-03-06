
#include "cgtensor.h"

CGTensors::CGTensors(const std::vector<int> &spins, const std::vector<Arrow> &dirs): valid_(false)
{
    //assert(spins.size()==dirs.size());

    for (int i=0; i<spins.size(); i++)
    {
        spin_legs_.push_back(IndexSpin(spins[i],dirs[i]));
    }

    init();
}

CGTensors::CGTensors(const std::vector<IQIndex> &sz_legs): valid_(false)
{
    for (const auto &leg : sz_legs)
    {
        spin_legs_.push_back(IndexSpin(leg));
    }

    init();
}

CGTensors::CGTensors(const std::vector<IndexSpin> &spin_legs): spin_legs_(spin_legs), valid_(false)
{
    init();
}




void CGTensors::init()
{
    
    ////filter spins according to their direction (Out and In)
    //std::vector<int> out_spins(spin_qns_.size()),
    //                 in_spins(spin_qns_.size());

    //auto out_spins_iter=std::copy_if(spin_qns_.begin(),spin_qns_.end(),out_spins.begin(),
    //        [](int qn) { return (qn.dir()==Out) } );
    //out_spins.resize(std::distance(out_spins.begin(),out_spins_iter));

    //auto in_spins_iter=std::copy_if(spin_qns_.begin(),spin_qns_.end(),in_spins.begin(),
    //        [](int qn) { return (qn.dir()==In) } );
    //in_spins.resize(std::distance(in_spins.begin(),in_spins_iter));
    //assert(out_spins.size()+in_spins.size()==spin_qns_.size());

    ////to obtain all sets of different mediate spins
    ////# of sets = # of fusion channel
    ////mediate_spins_sets are used to decompose K's to CG coefficient
    //std::vector<std::vector<int> > mediate_spins_sets;

    //valid_=obtain_mediate_spins(out_spins,in_spins,mediate_spins_sets);

    //if (!valid_)
    //    return;

    //filter spin legs according to their direction
    std::vector<IndexSpin> out_spin_legs(spin_legs_.size()), 
                           in_spin_legs(spin_legs_.size());

    auto out_spin_legs_iter=std::copy_if(spin_legs_.begin(),spin_legs_.end(),out_spin_legs.begin(),
            [](IndexSpin &spin_leg) { return (spin_leg.dir()==Out); } );
    out_spin_legs.resize(std::distance(out_spin_legs.begin(),out_spin_legs_iter));
    
    auto in_spin_legs_iter=std::copy_if(spin_legs_.begin(),spin_legs_.end(),in_spin_legs.begin(),
            [](IndexSpin &spin_leg) { return (spin_leg.dir()==In); } );
    in_spin_legs.resize(std::distance(in_spin_legs.begin(),in_spin_legs_iter));

    assert(out_spin_legs.size()+in_spin_legs.size()==spin_legs_.size());

    valid_=obtain_K(out_spin_legs,in_spin_legs,K_);

    //make tensors with norm 1.
    //for (auto &tensor : K_) tensor/=tensor.norm();
}


bool CGTensors::obtain_K(const std::vector<IndexSpin> &out_spin_legs, const std::vector<IndexSpin> &in_spin_legs, std::vector<IQTensor> &K)
{
    //only out_spin_legs, then consider spin singlet states formed by out spin legs
    if (in_spin_legs.size()==0)
    {
        IndexSpin in_spin_zero_leg(0,In);
        bool valid_spin_legs=obtain_K(out_spin_legs,std::vector<IndexSpin>{in_spin_zero_leg},K);

        //eliminate the in_spin_zero_leg from K obtained above
        if (valid_spin_legs)
        {
            in_spin_zero_leg.dag();
            IQTensor spin_zero_tensor(in_spin_zero_leg(1));
            for (auto &tensor : K)
                tensor*=spin_zero_tensor;
        }

        return valid_spin_legs;
    }

    //only in_spin_legs, then consider spin singlets formed by in_spin_legs
    if (out_spin_legs.size()==0)
    {
        IndexSpin out_spin_zero_leg(0,Out);
        bool valid_spin_legs=obtain_K(std::vector<IndexSpin>{out_spin_zero_leg},in_spin_legs,K);

        //eliminate the out_spin_zero_leg from K
        if (valid_spin_legs)
        {
            out_spin_zero_leg.dag();
            IQTensor spin_zero_tensor(out_spin_zero_leg(1));
            for (auto &tensor : K)
                tensor*=spin_zero_tensor;
        }

        return valid_spin_legs;
    }

    //one in spin and one out spin, then these two must equal
    //If in spin equal to out spin, K only have one choice, which should be delta function of sz
    //otherwise K vanishes
    if (in_spin_legs.size()==1 && out_spin_legs.size()==1)
    {
        auto out_spin_leg=out_spin_legs[0], in_spin_leg=in_spin_legs[0];
        if (in_spin_leg.spin_qn()!=out_spin_leg.spin_qn())
            return false;

        K.push_back(IQTensor(out_spin_leg.leg(),in_spin_leg.leg()));

        for (int sz=out_spin_leg.spin_qn(); sz>=-out_spin_leg.spin_qn(); sz-=2)
        {
            K[0](out_spin_leg.indval(sz),in_spin_leg.indval(sz))=1;
        }

        return true;
    }

    //one in spin and multiple out spins case, then fuse out spins to in spin
    //use recursive method to do CG decomposition
    if (in_spin_legs.size()==1)
    {
        auto &in_spin_leg=in_spin_legs[0];
        int max_fusion_spin=0;

        for (auto &spin_leg: out_spin_legs)
        {
            max_fusion_spin+=spin_leg.spin_qn();
        }

        if (in_spin_leg.spin_qn()>max_fusion_spin) return false;
        if (in_spin_leg.spin_qn()%2!=max_fusion_spin%2) return false;

        if (out_spin_legs.size()==2)
        {
            int min_fusion_spin=std::abs(out_spin_legs[0].spin_qn()-out_spin_legs[1].spin_qn());
            if (in_spin_leg.spin_qn()<min_fusion_spin) return false;

            K.push_back(obtain_CG(out_spin_legs[0],out_spin_legs[1],in_spin_leg));

            return true;
        }

        if (out_spin_legs.size()>2)
        {
            IndexSpin last_out_spin_leg=out_spin_legs.back();
            std::vector<IndexSpin> out_spin_legs_exclude_last=out_spin_legs;
            out_spin_legs_exclude_last.pop_back();

            bool valid_spin_legs=false;

            int mediate_spin_min=std::abs(in_spin_leg.spin_qn()-last_out_spin_leg.spin_qn());
            int mediate_spin_max=in_spin_leg.spin_qn()+last_out_spin_leg.spin_qn();
            for (int mediate_spin_qn=mediate_spin_min; mediate_spin_qn<=mediate_spin_max; mediate_spin_qn+=2)
            {
                IndexSpin mediate_spin_leg(mediate_spin_qn,In);
                std::vector<IQTensor> K_exclude_last;

                if (obtain_K(out_spin_legs_exclude_last,std::vector<IndexSpin>{mediate_spin_leg},K_exclude_last))
                {
                    valid_spin_legs=true;
                    mediate_spin_leg.dag();
                    IQTensor last_K=obtain_CG(mediate_spin_leg,last_out_spin_leg,in_spin_leg);

                    //Print(out_spin_legs_exclude_last);
                    //Print(mediate_spin_leg);
                    //PrintDat(K_exclude_last);
                    //PrintDat(last_K);

                    for (auto &tensor_K : K_exclude_last)
                    {
                        tensor_K*=last_K;
                    }

                    K.insert(K.end(),K_exclude_last.begin(),K_exclude_last.end());
                }
            }

            return valid_spin_legs;
        }
    }

    //one out spin and many in spins, then splits in spins to out spin
    if (out_spin_legs.size()==1)
    {
        auto out_spin_leg=out_spin_legs[0];
        int max_fusion_spin=0;

        for (auto &spin_leg: in_spin_legs)
        {
            max_fusion_spin+=spin_leg.spin_qn();
        }
        
        if (out_spin_leg.spin_qn()>max_fusion_spin) return false;
        if (out_spin_leg.spin_qn()%2!=max_fusion_spin%2) return false;

        if (in_spin_legs.size()==2)
        {
            int min_fusion_spin=std::abs(in_spin_legs[0].spin_qn()-in_spin_legs[1].spin_qn());
            if (out_spin_leg.spin_qn()<min_fusion_spin) return false;

            K.push_back(obtain_CG(in_spin_legs[0],in_spin_legs[1],out_spin_leg));

            return true;
        }

        if (in_spin_legs.size()>2)
        {
            IndexSpin last_in_spin_leg=in_spin_legs.back();
            std::vector<IndexSpin> in_spin_legs_exclude_last=in_spin_legs;
            in_spin_legs_exclude_last.pop_back();

            bool valid_spin_legs=false;

            int mediate_spin_min=std::abs(out_spin_leg.spin_qn()-last_in_spin_leg.spin_qn());
            int mediate_spin_max=out_spin_leg.spin_qn()+last_in_spin_leg.spin_qn();
            for (int mediate_spin_qn=mediate_spin_min; mediate_spin_qn<=mediate_spin_max; mediate_spin_qn+=2)
            {
                IndexSpin mediate_spin_leg(mediate_spin_qn,Out);
                std::vector<IQTensor> K_exclude_last;

                if (obtain_K(std::vector<IndexSpin>{mediate_spin_leg},in_spin_legs_exclude_last,K_exclude_last))
                {
                    valid_spin_legs=true;
                    mediate_spin_leg.dag();
                    IQTensor last_K=obtain_CG(mediate_spin_leg,last_in_spin_leg,out_spin_leg);

                    for (auto &tensor_K : K_exclude_last)
                    {
                        tensor_K*=last_K;
                    }

                    K.insert(K.end(),K_exclude_last.begin(),K_exclude_last.end());
                }
            }

            return valid_spin_legs;

        }
    }

    //multiple out spins and in spins, then fuse out spins to some mediate spin and split that meidate spin to in spins
    if (in_spin_legs.size()>1 && out_spin_legs.size()>1)
    {
        int sum_out_spins_qn=0, sum_in_spins_qn=0;

        for (auto &spin_leg : out_spin_legs)
        {
            sum_out_spins_qn+=spin_leg.spin_qn();
        }
        
        for (auto &spin_leg : in_spin_legs)
        {
            sum_in_spins_qn+=spin_leg.spin_qn();
        }

        //(half-)integer spins must fuse to (half-)integer
        if (sum_out_spins_qn%2!=sum_in_spins_qn%2) return false;

        bool valid_spin_legs=false;

        int max_spin_qn=std::min(sum_out_spins_qn,sum_in_spins_qn);

        for (int mediate_spin_qn=0; mediate_spin_qn<=max_spin_qn; mediate_spin_qn+=2)
        {
            std::vector<IQTensor> K_out, K_in;
            bool valid_out_spin_legs=false, valid_in_spin_legs=false;

            IndexSpin mediate_spin_leg(mediate_spin_qn,In);
            valid_out_spin_legs=obtain_K(out_spin_legs,std::vector<IndexSpin>{mediate_spin_leg},K_out);

            if (valid_out_spin_legs)
            {
                mediate_spin_leg.dag();
                valid_in_spin_legs=obtain_K(std::vector<IndexSpin>{mediate_spin_leg},in_spin_legs,K_in);

                if (valid_in_spin_legs)
                {
                    valid_spin_legs=true;

                    for (auto &tensorK_out : K_out)
                        for (auto &tensorK_in : K_in)
                            K.push_back(tensorK_out*tensorK_in);
                }
            }
        }

        return valid_spin_legs;
    }

    cout << "Miss some cases when constructing K!" << endl;
    return false;
}


IQTensor obtain_CG(const IndexSpin &S1, const IndexSpin &S2, const IndexSpin &S3)
{
    IQTensor CG(S1.leg(),S2.leg(),S3.leg());

    for (auto m1=S1.spin_qn(); m1>=-S1.spin_qn(); m1-=2)
    {
        for (auto m2=S2.spin_qn(); m2>=-S2.spin_qn(); m2-=2)
        {
            for (auto m3=S3.spin_qn(); m3>=-S3.spin_qn(); m3-=2)
            {
                if (m1+m2!=m3) continue;
                int j1=S1.spin_qn(), j2=S2.spin_qn(), j3=S3.spin_qn();

                CG(S1.indval(m1),S2.indval(m2),S3.indval(m3))=gsl_sf_coupling_3j(j1,j2,j3,m1,m2,-m3)*sqrt(j3+1.)*pow(-1.,(j1-j2+m3)/2.);
            }
        }
    }

    return CG;
}


