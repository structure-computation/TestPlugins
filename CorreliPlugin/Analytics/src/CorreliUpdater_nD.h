#ifndef CORRELIUPDATER_ND_H
#define CORRELIUPDATER_ND_H

#include <QtCore/QTemporaryFile>
#include <QtCore/QByteArray>
#include <QtCore/QBuffer>
#include <QtGui/QImage>

#include "Soca/Model/TypedArray.h"
#include "Soca/Com/SodaClient.h"

#include "CorreliUpdater.h"
#include "IntegratedDic.h"

#include <correlation/crack_cut.h>
#include <errno.h>
#include <vector>


struct GetEps {
    template<class TE,class TD>
    void operator()( const TE &e, int i, TD *data, int d ) const {
        data->operator[]( i ) = e.epsilon[ 0 ][ d ];
    }
};


inline void _save_img( MP mp, const QImage &res, int j ) {
    // -> png
    QByteArray ba;
    QBuffer buffer( &ba );
    buffer.open( QIODevice::WriteOnly );
    res.save( &buffer, "PNG" );

    // -> base64
    QByteArray b6;
    b6.append( "data:image/png;base64," );
    b6.append( ba.toBase64() );

    MP field = MP::new_obj( "ImageField" );
    field[ "src" ] = QString::fromAscii( b6.data(), b6.size() );

    // Interpolated field fill-in
    MP disp = MP::new_obj();
    disp[ "field" ] = field;
    disp[ "pos" ] = MP::new_lst();
    MP pos_v = MP::new_obj();
    pos_v[ "axe_value" ] = j;
    pos_v[ "axe_name" ] = "time";
    disp[ "pos" ] << pos_v;

    mp[ "data._data" ] << disp;
}

inline void check_image_field( MP mp, QString n, QString N ) {
    for( int i = 0; ; ++i ) {
        if ( i == mp[ "visualization.color_by.lst" ].size() ) {
            mp[ "visualization.color_by.lst" ] << mp[ n ];
            break;
        }
        MP l = mp[ "visualization.color_by.lst" ][ i ];
        if ( QString( l[ "name" ] ) == N )
            break;
    }
    mp[ n + ".data._data" ].clear();
}

template<class TM>
MP soda_mesh_from_lmtpp_mesh( const TM &m ) {
    MP om = MP::new_obj( "Mesh" );
    om[ "points" ] = MP::new_lst( "Lst_Point" );
    om[ "_elements" ] = MP::new_lst();
    om[ "_selected_points" ] = MP::new_lst();
    om[ "_pelected_points" ] = MP::new_lst();
    om[ "_selected_elements" ] = MP::new_lst();
    om[ "_pelected_elements" ] = MP::new_lst();

    // nodes
    for( int i = 0; i < m.node_list.size(); ++i ) {
        MP pos = MP::new_lst( "Vec_3" );
        for( int d = 0; d < 3; ++d )
            pos << m.node_list[ i ].pos[ d ];

        MP pnt = MP::new_obj( "Point" );
        pnt[ "pos" ] = pos;

        om[ "points" ] << pnt;
    }

    // elements
    TypedArray<int> *tr_con = new TypedArray<int>;
    TypedArray<int> *te_con = new TypedArray<int>;
    for( int n = 0; n < m.elem_list.size(); ++n ) {
        if ( m.elem_list[ n ]->nb_nodes_virtual() == 3 )
            for( int i = 0; i < m.elem_list[ n ]->nb_nodes_virtual(); ++i )
                tr_con->_data << m.elem_list[ n ]->node_virtual( i )->number;
        else if ( m.elem_list[ n ]->nb_nodes_virtual() == 4 )
            for( int i = 0; i < m.elem_list[ n ]->nb_nodes_virtual(); ++i )
                te_con->_data << m.elem_list[ n ]->node_virtual( i )->number;
    }

    for( int n = 0; n < m.skin.elem_list.size(); ++n )
        if ( m.skin.elem_list[ n ]->nb_nodes_virtual() == 3 )
            for( int i = 0; i < m.skin.elem_list[ n ]->nb_nodes_virtual(); ++i )
                tr_con->_data << m.skin.elem_list[ n ]->node_virtual( i )->number;

    // triangle
    tr_con->_size.resize( 2 );
    tr_con->_size[ 0 ] = 3;
    tr_con->_size[ 1 ] = tr_con->_data.size() / 3;

    MP triangles = MP::new_obj( "Element_TriangleList" );
    triangles[ "indices" ] = tr_con;
    om[ "_elements" ] << triangles;

    // tetra
    te_con->_size.resize( 2 );
    te_con->_size[ 0 ] = 4;
    te_con->_size[ 1 ] = te_con->_data.size() / 4;

    MP tetrahedra = MP::new_obj( "Element_TetrahedraList" );
    tetrahedra[ "indices" ] = te_con;
    om[ "_elements" ] << tetrahedra;

    return om;
}

inline MP load_file_correli(CorreliUpdater *updater, MP data_file){
//chargement du fichier contenant les données d'output
    quint64 ptr = data_file[ "_ptr" ];
    QString name = data_file[ "name" ];
    MP output_field = updater->sc->load_ptr( ptr );
    return output_field;
}


template<unsigned dim>
bool correliUpdater_nD( CorreliUpdater *updater, MP mp, LMT::Number<dim> ) {
    typedef ImgInterp<double,dim/*,ImgInterpOrder3_Kernel*/> I2;
    typedef LMT::DicCPU<double,dim/*,false*/> DC2;
    typedef Mesh<Mesh_carac_elasticity_dep<double,dim> > TM2;
    typedef Formulation<TM2,elasticity_dep> TF2;
    typedef Formulation<TM2,laplacian     > TB2;
    typedef typename TM2::Pvec Pvec;

    DC2 dic;
    TM2 dic_mesh;
    LMT::Vec<I2> images;
    TF2 form( dic_mesh );
    TB2 bord( dic_mesh );
    
    int size_alternative_loading = 20; // number of pictures over which the images are loaded one by one after each DIC step
    std::vector<QString> vec_names;
    int tot_num_im=0; //total number of images
    


    
    // load data
    MP mesh;
    MP ch = mp[ "_children" ];
    Vec<Vec<Pvec,2> > contrained_lines;
    for( int i = 0; i < ch.size(); ++i ) {
        MP c = ch[ i ];
        qDebug() <<  c.type() ;
        if ( c.type() == "ImgSetItem" ) {
            tot_num_im += c[ "_children" ].size();
            vec_names.resize(tot_num_im);
            for( int j = 0; j < c[ "_children" ].size(); ++j ) {
                MP im = c[ "_children" ][ j ];
                if ( im.type() == "ImgItem" ) {
                    I2 *img = images.new_elem();
                    QString name = im[ "img.src" ];
                    if ( im[ "img.src" ].type() != "Path" )
                        name = "../CorreliOnline/html/" + name;
                    
                    if ((tot_num_im > size_alternative_loading) and (j>0))
                        vec_names[i] = name;
                    else {
                        vec_names[i] = name;
                        try {
                            img->load( name.toAscii().data() );
                            img->reverse_y();
                        } catch ( std::string msg ) {
                            updater->add_message( mp, Updater::ET_Error, "Img " + name + " does not exist" );
                            return false;
                        }
                    }
                } else if ( im.type() == "RawVolume" ) {
                    I2 *img = images.new_elem();
                    MP ms( im[ "img_size" ] );
                    Vec<int,3> S( ms[ 0 ], ms[ 1 ], ms[ 2 ] );
                    PRINT( S );

                    MP volume = im[ "_children" ][ 0 ];
                    qDebug() << volume;
                    MP data = updater->sc->load_ptr( volume[ "_ptr" ] );
                    qDebug() << data;

                    QString name = data;
                    PRINT( name.toAscii().data() );

                    try {
                        typedef unsigned char PI8;
                        img->template load_binary<PI8>( name.toAscii().data(), S );
                    } catch ( std::string msg ) {
                        updater->add_message( mp, Updater::ET_Error, "Img " + name + " does not exist" );
                        return false;
                    }
                    PRINT( name.toAscii().data() );
                }
            }
        }
        else if ( c.type() == "ImgFieldSetDirectoryItem"){
            MP ch = c[ "_children" ];
            tot_num_im += ch.size();
            vec_names.resize(tot_num_im);
            
            for( int j = 0; j < ch.size(); ++j ) {
            //for( int j = 0; j < 3; ++j ) {
                I2 *img = images.new_elem();
                MP c = ch[ j ];
                MP file = c["_file"];
                quint64 ptr = file[ "_ptr" ];
                QString name = file[ "name" ];
                MP data = updater->sc->load_ptr( ptr );
                if ( data.ok() and data.type() == "Path" ) {
                    int rem = file[ "_info.remaining" ];
                    if ( not rem ) {
                        // read
                        QString name = data;
                        
                        if ((tot_num_im > size_alternative_loading) and (j>0))
                            vec_names[j] = name;
                        else {
                            vec_names[j] = name;
                            try {
                                img->load( name.toAscii().data() );
                                img->reverse_y();
                            } catch ( std::string msg ) {
                                updater->add_message( mp, Updater::ET_Error, "Img " + name + " does not exist" );
                                return false;
                            }
                        }
                    }
                }
            }  
        }
        if ( c.type() == "DiscretizationItem" ) {
            mesh.reassign( c[ "_children[ 0 ]._mesh" ] );
            for( int i = 0, n = mesh[ "points" ].size(); i < n; ++i ) {
                MP pos = mesh[ "points" ][ i ][ "pos" ];
                dic_mesh.add_node( Pvec( pos[ 0 ], pos[ 1 ], pos[ 2 ] ) );
            }

            for( int nel = 0, mel = mesh[ "_elements" ].size(); nel < mel; ++nel ) {
                MP el = mesh[ "_elements" ][ nel ];
                if ( el.type() == "Element_TriangleList" and dim == 2 ) {
                    if ( TypedArray<int> *indices = dynamic_cast<TypedArray<int> *>( el[ "indices" ].model() ) ) {
                        for( int nt = 0, ct = 0; nt < indices->size( 1 ); ++nt ) {
                            unsigned o[ 3 ];
                            for( int j = 0; j < 3; ++j )
                                o[ j ] = qMin( indices->operator[]( ct++ ), (int)dic_mesh.node_list.size() - 1 );
                            dic_mesh.add_element( Triangle(), DefaultBehavior(), o );
                        }
                    }
                } else if ( el.type() == "Element_TetrahedraList" and dim == 3 ) {
                    if ( TypedArray<int> *indices = dynamic_cast<TypedArray<int> *>( el[ "indices" ].model() ) ) {
                        for( int nt = 0, ct = 0; nt < indices->size( 1 ); ++nt ) {
                            unsigned o[ 4 ];
                            for( int j = 0; j < 4; ++j )
                                o[ j ] = qMin( indices->operator[]( ct++ ), (int)dic_mesh.node_list.size() - 1 );
                            dic_mesh.add_element( Tetra(), DefaultBehavior(), o );
                        }
                    }
                }
            }
        }
        if ( c.type() == "PhysicsItem" ) {
            MP pc = c[ "_children" ];
            for( int cp = 0; cp < pc.size(); ++cp ) {
                if ( pc[ cp ].type() == "BorderDisplacementItem" ) {
                    MP bc = pc[ cp ][ "_children" ];
                    for( int j = 0; j < bc.size(); ++j ) {
                        if ( bc[ j ].type() == "PickedZoneItem" ) {
                            MP pe = bc[ j ][ "picked_element" ][ 0 ];
                            MP mesh = pe[ "mesh" ];
                            MP elem = pe[ "element" ];
                            if ( elem.type() == "Element_Line" ) {
                                int i0 = elem[ "indices" ][ 0 ];
                                int i1 = elem[ "indices" ][ 1 ];
                                MP P0 = mesh[ "points" ][ i0 ][ "pos" ];
                                MP P1 = mesh[ "points" ][ i1 ][ "pos" ];
                                contrained_lines << vec(
                                    Pvec( P0[ 0 ], P0[ 1 ], P0[ 2 ] ),
                                    Pvec( P1[ 0 ], P1[ 1 ], P1[ 2 ] )
                                );
                            } else
                                qDebug() << "Unmanaged element type" << elem.type();
                        }
                    }
                }
            }
        }
    }

      PRINT(images.size());
      PRINT(tot_num_im);
    // regularization
    if ( double r = mp[ "parameters.lambda_bulk" ] ) {
        dic.form_reg = &form;
        dic.form_bor = &bord;
        dic.default_lambda = r;
        get_constrained_nodes( dic.constrained_nodes, dic_mesh, contrained_lines );
    }

    //
    Pvec rb_guess( mp[ "parameters.rb_guess" ][ 0 ],  mp[ "parameters.rb_guess" ][ 1 ],  mp[ "parameters.rb_guess" ][ 2 ] );
    for( int i = 0; i < dic_mesh.node_list.size(); ++i )
        dic_mesh.node_list[ i ].dep = rb_guess;
    PRINT( rb_guess );

    
    
    
// ------------------------------------------------------------------------------------------------------------------------------
    
    // Dossier contenant les FileItem poitant vers les fichiers résultats
    
//     MP data_directory = updater->sc->load_ptr( mp[ "_output[0]" ][ "_img_field_set_ptr" ]);
    MP data_directory = mp[ "_children[1]" ];
    PRINT( data_directory.size() );
    
    // output
    MP output_visu = mp[ "_output[ 0 ]" ];
    
    // RaZ du temps
    mp[ "_output[ 0 ]" ][ "time.val" ] = 0;

    
    double t1 = time_of_day_in_sec();
    double t2 = time_of_day_in_sec();

    
    for( int j = 1; j <tot_num_im; ++j ) {

        
        //chargement d'un fichier résultats
        MP output_field = updater->sc->load_ptr( data_directory[ "_children" ][j][ "_result_ptr" ]);
//         MP data_file = output_visu[ "_file_field_set_data" ];
//         MP output_field = load_file_correli(updater, data_file);
        
        
        // initialisation des outputs --------------------------------------------------------------------------------
        // output_field pour le conteneur de data
        QVector<MP> displacements = make_field( output_field, dic_mesh.dim, "Displacement" );
        MP uncertainty = make_field( output_field, "Uncertainty" );

        QVector<QString> l;
        l << "xx" << "yy" << "xy";
        QVector<MP> epsilon = make_field( output_field, 3, "Strain", l );

        // residual_adv
        check_image_field( output_field, "_residual_adv"    , "Residual adv"     );
        check_image_field( output_field, "_residual"        , "Residual"         );
        check_image_field( output_field, "_residual_int_adv", "Residual int adv" );
        check_image_field( output_field, "_residual_int"    , "Residual int"     );
        
        // output_visu pour le conteneur de visualisation
        QVector<MP> temp_displacements      = make_field( output_visu, dic_mesh.dim, "Displacement" );
        MP          temp_Uncertainty        = make_field( output_visu, "Uncertainty" );

        QVector<MP> temp_epsilon            = make_field( output_visu, 3, "Strain", l );

        // residual_adv
        check_image_field( output_visu, "_residual_adv"    , "Residual adv"     );
        check_image_field( output_visu, "_residual"        , "Residual"         );
        check_image_field( output_visu, "_residual_int_adv", "Residual int adv" );
        check_image_field( output_visu, "_residual_int"    , "Residual int"     );
        

        // execution -------------------------------------------------------------------------------------------------------------------
        dic.min_norm_inf_dU  = mp[ "parameters.norm_inf"    ].ok() ? mp[ "parameters.norm_inf"    ] : 0.0;
        dic.min_norm_2_dU    = mp[ "parameters.norm_2"      ].ok() ? mp[ "parameters.norm_2"      ] : 0.0;
        dic.max_cpt_iter     = mp[ "parameters.nb_iter_max" ].ok() ? mp[ "parameters.nb_iter_max" ] : 100;
        dic.max_displacement = mp[ "parameters.max_displacement" ].ok() ? mp[ "parameters.max_displacement" ] : -1;
        dic.levenberg_marq   = 1e-10; ///< lonely nodes

        dic.multi_resolution = mp[ "parameters.multi_res.val" ];
        PRINT( dic.multi_resolution );

        // uncertainty
        if ( int( mp[ "parameters.uncertainty" ] ) ) {
            dic.get_uncertainty( images[ 0 ], dic_mesh, dep_DM(), lum_DM(), 1, 100 );

            QVector<int> s; s << dic.uncertainty.size();
            TypedArray<double> *data = new TypedArray<double>( s );
            for( int i = 0; i < dic.uncertainty.size(); ++i )
                data->operator[]( i ) = dic.uncertainty[ i ];

            add_field_in_Interpolated( uncertainty, mesh, data, 0 );
        }

        // standard dic
        dic_mesh.update_skin();
        
        // For integrated
    //  Vec<Vec<Pvec> > dep; dep.resize(tot_num_im - 1 );
    //  Vec<TM2> mesh_vec; mesh_vec.resize(tot_num_im - 1 );
        

        double t0 = time_of_day_in_sec();
        int delay;
        if ( dim == 3 )
            dic.rotation_in_rigid_body = false;        
    
    
    
        double bef = time_of_day_in_sec();
        //if (j == 1)
        std::cout << vec_names.size() << std::endl;
        if (tot_num_im > size_alternative_loading){
            images.resize(1);
            images.resize(2);
            images[1].load(vec_names[j].toAscii().data() );
            images[1].reverse_y();
            dic.exec_rigid_body( images[ 0 ], images[ 1 ], dic_mesh, dep_DM(), lum_DM() );
        }
        else 
            dic.exec_rigid_body( images[ 0 ], images[ j ], dic_mesh, dep_DM(), lum_DM() );
        
        PRINT("Starting with image :" + to_string(j));
        PRINT(dic_mesh.node_list[0].dep);
        
        if (tot_num_im > size_alternative_loading)
            dic.exec( images[ 0 ], images[ 1 ], dic_mesh, dep_DM(), lum_DM() );
        else 
            dic.exec( images[ 0 ], images[ j ], dic_mesh, dep_DM(), lum_DM() );
        
        std::cout << " " << std::endl;
        PRINT("Image " + to_string(j) + " treated");
        std::cout << " " << std::endl;
        PRINT(dic_mesh.node_list[0].dep);

        I2 residual_img, residual_adv_img;
        if ( dim == 2 ) {
            if (tot_num_im > size_alternative_loading){
                dic.get_residual_adv_img( images[ 0 ], images[ 1 ], dic_mesh, dep_DM(), lum_DM(), residual_adv_img );
                dic.get_residual_img    ( images[ 0 ], images[ 1 ], dic_mesh, dep_DM(), lum_DM(), residual_img     );
            }
            else{
                dic.get_residual_adv_img( images[ 0 ], images[ j ], dic_mesh, dep_DM(), lum_DM(), residual_adv_img );
                dic.get_residual_img    ( images[ 0 ], images[ 1 ], dic_mesh, dep_DM(), lum_DM(), residual_img     );
            }
        }

   //     for( int i = 0; i < dic_mesh.node_list.size(); ++i )
  //          dep[ j - 1 ] << dic_mesh.node_list[ i ].dep;

  //      mesh_vec[ j - 1 ] = dic_mesh;

        // crack ?
        double crack_eps_threshold = mp[ "parameters.crack_eps_threshold" ];
        if ( crack_eps_threshold > 0 and j == 1 ) {
            for( int o = 0; o < dic_mesh.node_list.size(); ++o )
                dic_mesh.node_list[ o ].dep_sv = dic_mesh.node_list[ o ].dep;
        double t1 = time_of_day_in_sec();

            dic.default_lambda *= 1.1;
            if (tot_num_im > size_alternative_loading)
                dic.exec( images[ 0 ], images[ 1 ], dic_mesh, dep_DM(), lum_DM() );
            else 
                dic.exec( images[ 0 ], images[ j ], dic_mesh, dep_DM(), lum_DM() );

            for( int o = 0; o < dic_mesh.node_list.size(); ++o )
                dic_mesh.node_list[ o ].dep_sv -= dic_mesh.node_list[ o ].dep;

            Pvec dir( 0 ); dir[ 0 ] = 1;
            if ( mp[ "parameters.crack_dir" ].ok() )
                for( int d = 0; d < dic_mesh.dim; ++d )
                    dir[ d ] = mp[ "parameters.crack_dir" ][ d ];

            crack_cut( dic_mesh, crack_eps_threshold, dir );
            for( int o = 0; o < dic_mesh.node_list.size(); ++o )
                dic_mesh.node_list[ o ].dep = rb_guess;

            dic.default_lambda /= 1.1;
            if (tot_num_im > size_alternative_loading)
                dic.exec( images[ 0 ], images[ 1 ], dic_mesh, dep_DM(), lum_DM() );
            else 
                dic.exec( images[ 0 ], images[ j ], dic_mesh, dep_DM(), lum_DM() );

            dic_mesh.update_skin();
            mesh.reassign( soda_mesh_from_lmtpp_mesh( dic_mesh ) );
        }



        // disp field
        for( int d = 0; d < dic_mesh.dim; ++d ) {
            // data
            QVector<int> s; s << dic_mesh.node_list.size();
            TypedArray<double> *data = new TypedArray<double>( s );

            for( int i = 0; i < dic_mesh.node_list.size(); ++i )
                data->operator[]( i ) = dic_mesh.node_list[ i ].dep[ d ];

            // NodalField
            add_field_in_Interpolated( displacements[ d ], mesh, data, j-1 );
        }

        // epsilon field
        for( int d = 0; d < 3; ++d ) {
            // data
            QVector<int> s; s << dic_mesh.elem_list.size();
            TypedArray<double> *data = new TypedArray<double>( s );

            apply_wi( dic_mesh.elem_list, GetEps(), data, d );

            // NodalField
            add_field_in_Interpolated( epsilon[ d ], mesh, data, j-1, "ElementaryField" );
        }

        // residual
        if ( dim == 2 ) {
            _save_img( output_field[ "_residual_adv" ], residual_adv_img.to_QImage( true ), j-1 );
            _save_img( output_field[ "_residual"     ], residual_img    .to_QImage( true ), j-1 );
        }

        // display
        double aft = time_of_day_in_sec();

        QString intermediate_time = QString("%1").arg( aft - bef );
        QString iter = QString("%1").arg( j );
        updater->add_message( mp, Updater::ET_Info, "Correlation " + iter + " done in " + intermediate_time + "s" );
        
//         MP test0 = mp[ "_output[ 0 ].visualization.color_by.lst" ][ 0 ];
// //         qDebug() << "output_field : " << test0["data"];
// //         qDebug() << "output_field : " << test0["data._data.pos.pos"];
//         for( int ii = 0; ii < test0["data._data"].size() ; ++ii ) {
//            MP test1 = test0["data._data"][ii];
//            qDebug() << "output_field : " << test1["pos"]; 
//           
//         }
        
//         mp.flush();
//         delay = 0.1;
//         sleep(delay);
        
        


        t2 += aft - bef;

        // save history curves
        output_field[ "_norm_i_history"   ].clear();
        output_field[ "_norm_2_history"   ].clear();
        output_field[ "_residual_history" ].clear();
        for( int i = 0; i < dic.history_norm_inf_dU           .size(); ++i ) output_field[ "_norm_i_history"   ] << dic.history_norm_inf_dU           [ i ];
        for( int i = 0; i < dic.history_norm_2_dU             .size(); ++i ) output_field[ "_norm_2_history"   ] << dic.history_norm_2_dU             [ i ];
        for( int i = 0; i < dic.history_dimensionless_residual.size(); ++i ) output_field[ "_residual_history" ] << dic.history_dimensionless_residual[ i ];

        
//         mp.flush();
        //    display( mesh_vec[ 0 ] );

        // identification ?
        //if ( int( mp[ "parameters.ident" ] ) and ( contrained_lines.size() or dic_mesh.dim == 3 ) )
        //  integrated_dic( mp, mesh, dic, mesh_vec, images, dep, contrained_lines );

    }
    
    QString global_time = QString("%1").arg( t2 - t1 );
    
    QString num_pic = QString("%1").arg(tot_num_im );
    updater->add_message( mp, Updater::ET_Success, "Correlation of " + num_pic + " pictures computed in " + global_time + "s" );
    
    // Initialisation de la visualisation
    mp[ "_output[ 0 ]" ][ "time.val" ] = 1;
    mp.flush();


    
    
    
    return true;
}



#endif // CORRELIUPDATER_ND_H