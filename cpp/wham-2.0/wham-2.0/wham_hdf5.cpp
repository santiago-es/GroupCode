#include "wham_hdf5.h"

#ifndef H5_NO_NAMESPACE
using namespace H5;
#endif

void Write_HDF5::init(t_options option,
                      t_wham args,
                      int n_exp,
                      int n_states,
                      std::vector<double> prob,
                      std::vector<double> pmf )
{
    options = option;
    wham_args = args;
    nexp = n_exp;
    nstates = n_states;
    opt_prob = prob;
    opt_pmf = pmf;
    char outname[1024];
    sprintf(outname,"%s",options.outname.c_str());
    std::stringstream ss;
    ss << outname;
    ss >> filename;
    const H5std_string FILE_NAME(outname);
    try
    {
        file = new H5File(FILE_NAME,H5F_ACC_TRUNC);
    }
    catch (FileIException error)
    {
        error.printError();
        exit(1);
    }
    catch( DataSetIException error )
    {
        error.printError();
        exit(1);
    }
    return;
}

void Write_HDF5::write_trj(std::vector<t_experiment> group_traj)
{
    try
    {
        std::cout << "Writing trajectory information to " << filename << "..." << std::endl;
        Group *grp;
        grp = new Group(file->createGroup("/Trajectories"));
        Group *tgrp;
        DataSpace *dataspace;
        DataSet *dataset;

        char name[1024];
        /* For each experiment */
        for (int i=0; i<nexp; i++)
        {
            sprintf(name,"/Trajectories/Traj-%i",i);
            std::cout << name << std::endl;
            tgrp = new Group(file->createGroup(name));

            // Trajectory frame biasing coordinate
            sprintf(name,"/Trajectories/Traj-%i/BiasingCoordinate",i);
            std::cout << name << std::endl;
            tgrp = new Group(file->createGroup(name));
            for (int j=0; j<options.ndof; j++)
            {
                hsize_t dimst[1];
                dimst[0] = group_traj[i].coordinate.size();
                double dat[group_traj[i].coordinate.size()];
                for (int k=0; k<group_traj[i].coordinate.size(); k++)
                {
                    dat[k] = group_traj[i].coordinate[k][j];
                }
                dataspace = new DataSpace(1,dimst);
                sprintf(name,"/Trajectories/Traj-%i/BiasingCoordinate/DoF-%i",i,j);
                std::cout << name << std::endl;
                dataset = new DataSet(file->createDataSet(name,PredType::NATIVE_DOUBLE, *dataspace));
                dataset->write(dat,PredType::NATIVE_DOUBLE);

                /* Attributes */
                double attr_data[4] = {group_traj[i].phi0[j],group_traj[i].deltaphi[j],group_traj[i].fc[j],group_traj[i].t};
                hsize_t att_dim[1] = { 4 };
                DataSpace attr_dataspace = DataSpace(1,att_dim);
                Attribute attribute = dataset->createAttribute("phi0 (degrees), dphi (degrees), force constant (kJ/mol/degree^2), temperature (K)",PredType::NATIVE_DOUBLE,attr_dataspace);
                attribute.write(PredType::NATIVE_DOUBLE, attr_data);

                /* close */
                dataset->close();
                delete dataset;
                delete dataspace;
            }
            // Trajectory frame bin 1D
            hsize_t dimst[1];
            dimst[0] = group_traj[i].coordinate.size();
            sprintf(name,"/Trajectories/Traj-%i/Bin",i);
            std::cout << name << std::endl;
            std::vector<int> bin_set(options.ndof);
            int b[group_traj[i].coordinate.size()];
            for (int j=0; j<group_traj[i].coordinate.size(); j++)
            {
                int tb = group_traj[i].frame_bin[j][0].binND;
                for (int k=0; k<options.ndof; k++)
                {
                    int tbd = group_traj[i].frame_bin[j][k].binND;;
                    if (tbd != tb)
                    {
                        std::cerr << "\nERROR!  Bin assignment for frame " << j << " degree of freedom " << k << " is " << tbd << " while the first degree of freedom is " << tb << ".  This should not be able to happen." << std::endl;
                    }
                }
                b[j] = tb;
            }
            dataspace = new DataSpace(1,dimst);
            dataset = new DataSet(file->createDataSet(name,PredType::NATIVE_INT, *dataspace));
            dataset->write(b,PredType::NATIVE_INT);
            dataset->close();
            delete dataset;
            delete dataspace;
            // Trajectory frame probability
            sprintf(name,"/Trajectories/Traj-%i/FrameProb",i);
            std::cout << name << std::endl;
            double p[group_traj[i].coordinate.size()];
            for (int j=0; j<group_traj[i].coordinate.size(); j++)
            {
                p[j] = opt_prob[b[j]]/(double)wham_args.counts[b[j]];
            }
            dataspace = new DataSpace(1,dimst);
            dataset = new DataSet(file->createDataSet(name,PredType::NATIVE_DOUBLE, *dataspace));
            dataset->write(p,PredType::NATIVE_DOUBLE);
            dataset->close();
            delete dataset;
            delete dataspace;
        }

    }
    catch (FileIException error)
    {
        error.printError();
        exit(1);
    }
    catch( DataSetIException error )
    {
        error.printError();
        exit(1);
    }
    return;
}

void Write_HDF5::write_wham(std::vector<double> prob,
                std::vector<double> pmf,
                std::vector<int> counts,
                std::vector<t_map> map,
                std::string group,
                int f0, int fN)
{
    char grpname[1024];
    try
    {
        DataSpace *dataspace;
        DataSet *dataset;
        std::cout << "Writing WHAM output information to " << filename << "..." << std::endl;
        Group *grp = new Group(file->createGroup(group));
        
        /* Attributes */
        int attr_data[2] = {f0,fN};
        hsize_t att_dim[1] = { 2 };
        DataSpace attr_dataspace = DataSpace(1,att_dim);
        Attribute attribute = grp->createAttribute("First frame read, last frame read",PredType::NATIVE_INT,attr_dataspace);
        attribute.write(PredType::NATIVE_INT, attr_data);

        /* Probability */
        sprintf(grpname,"%s/Probability",group.c_str());
        std::cout << grpname << std::endl;
        hsize_t dims2d[2];
        dims2d[0] = nstates;
        dims2d[1] = options.ndof+1;
        double ldld[nstates][options.ndof+1];
        for (int i=0; i<nstates; i++)
        {
            for (int j=0; j<options.ndof; j++)
            {
                ldld[i][j] = (map[i].bin1D[j] * options.b[j]) + options.x0[j];
            }
            ldld[i][options.ndof] = prob[i];
        }
        dataspace = new DataSpace(2,dims2d);
        dataset = new DataSet(file->createDataSet(grpname,PredType::NATIVE_DOUBLE, *dataspace));
        dataset->write(ldld,PredType::NATIVE_DOUBLE);
        
        /* Attributes */
        StrType str_type(PredType::C_S1, 1024);
        char prob_attr[options.ndof+1][1024];
        hsize_t prob_attr_dim[1] = { options.ndof+1 };
        for (int j=0; j<options.ndof; j++)
        {
            sprintf(prob_attr[j],"DoF%i (degrees)",j);
        }
        sprintf(prob_attr[options.ndof],"probability");
        DataSpace prob_attr_dataspace(1,prob_attr_dim);
        Attribute prob_attribute = dataset->createAttribute("Units", str_type, prob_attr_dataspace);
        prob_attribute.write(str_type, prob_attr);
        
        dataset->close();
        delete dataset;
        delete dataspace;
        
        /* PMF */
        sprintf(grpname,"%s/PMF",group.c_str());
        std::cout << grpname << std::endl;
        hsize_t dims1d[1];
        dims1d[0] = nstates;
        double ld[nstates];
        for (int i=0; i<nstates; i++)
        {
            ld[i] = pmf[i];
        }
        dataspace = new DataSpace(1,dims1d);
        dataset = new DataSet(file->createDataSet(grpname,PredType::NATIVE_DOUBLE, *dataspace));
        dataset->write(ld,PredType::NATIVE_DOUBLE);
        
        /* Attributes */
        const H5std_string pmfwritebuf("kJ/mol");
        DataSpace pmf_attr_dataspace(H5S_SCALAR);
        Attribute pmf_attribute = dataset->createAttribute("Units", str_type, pmf_attr_dataspace);
        pmf_attribute.write(str_type, pmfwritebuf);

        dataset->close();
        delete dataset;
        delete dataspace;
        
        /* bin counts */
        sprintf(grpname,"%s/BinCounts",group.c_str());
        std::cout << grpname << std::endl;
        int n[nstates];
        for (int i=0; i<nstates; i++)
        {
            n[i] = counts[i];
        }
        dataspace = new DataSpace(1,dims1d);
        dataset = new DataSet(file->createDataSet(grpname,PredType::NATIVE_INT, *dataspace));
        dataset->write(n,PredType::NATIVE_INT);
        dataset->close();
        delete dataset;
        delete dataspace;
    }
    catch (FileIException error)
    {
        error.printError();
        exit(1);
    }
    catch( DataSetIException error )
    {
        error.printError();
        exit(1);
    }
    return;
}
