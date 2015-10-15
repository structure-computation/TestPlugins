#include <Soca/Com/SodaClient.h>
#include "JobList.h"
#include "ServerAssistedVisualizationUpdater.h"
#include "FileUpdater.h"
#include "ImgUpdater.h"

int main( int argc, char **argv ) {
    // connection
    SodaClient sc( QHostAddress::Any, 8890 );
    if ( not sc.connected() ) return 1;

    // type d'item observé

    sc.reg_type( "CorrelationItem" );
    sc.reg_type( "VirtualGaugeItem" );
    sc.reg_type( "GmshItem" );
    sc.reg_type( "StepReaderToNurbsItem" );
    sc.reg_type( "File" );
    sc.reg_type( "Img" );
    sc.reg_type( "ServerAssistedVisualization" );
    sc.reg_type( "FieldSetCorreliItem" );
    
    
    //liste des job en cours
    JobList job_list;
    
    // attente
    while ( SodaClient::Event event = sc.event() ) {
        MP mp = event.mp();
        //test : verification de l'existance d'un job, ou création du job si necessaire

        // qDebug() << "model : " << mp;
        // qDebug() << "recu ";
        if ( mp.type() == "ServerAssistedVisualization" ) {
            ServerAssistedVisualizationUpdater mu;
            mu.sc = &sc;
            mu.exec( mp );
        } else if ( mp.type() == "File" ) {
            FileUpdater fu;
            fu.sc = &sc;
            fu.exec( mp );
        } else if ( mp.type() == "Img" ) {
            ImgUpdater iu;
            iu.sc = &sc;
            iu.exec( mp );
        } else{
            job_list.kill_jobs();
            int i_job = job_list.find_job_index(mp, sc);
            
            if(i_job >= 0){
                qDebug() << "recu 1 ";
                qDebug() << "job_list.jobs.size() : " << job_list.jobs.size();
                Job *current_job;
                current_job = job_list.jobs[i_job];
                current_job->putLauncherInAThread(mp);
                
    //             mp[ "_computation_mode" ] = false;
            }else if(i_job == -1 ){
                qDebug() << "i_job : " << i_job ;
            }else if(i_job == -2 ){
                mp[ "_computation_mode" ]   = false;
                mp[ "_ready_state" ]        = false;
                mp[ "_computation_state" ]  = false;
                mp[ "_pending_state" ]      = false;
                mp[ "_processing_state" ]   = false;
                mp[ "_finish_state" ]       = false;
                mp[ "_stop_state" ]         = true;
                qDebug() << "i_job : " << i_job ;
                mp.flush();
            }
        }
       
       // ----------------------------------------------------
        // Maj visualisation : Copier un fichier résultat (FieldSetDataCorreliModel) dans visualisation (FieldSet)
        
//         if(mp.type() == "FieldSetCorreliItem"){
//             MP data_directory = sc.load_ptr( mp[ "_img_directory_ptr" ]);
//             qDebug() << "TEST 0 : " << data_directory.type();
//             
//             MP test1 = data_directory[ "_children[0]" ][ "_result_ptr" ];
//             qDebug() << "TEST 1 : " << test1;
//             MP test2 = data_directory[ "_children[1]" ][ "_result_ptr" ];
//             qDebug() << "TEST 2 : " << test2;
//         
// //               MP output_visu = mp[ "_output[0]" ];
//             MP list_visualisation = mp["visualization.color_by.lst"];
//             
//             int time = mp[ "time.val" ];
//             int old_time = mp[ "_old_time_step" ];      
//             
//             if(time != old_time){
// 
//                 MP output_field = sc.load_ptr(data_directory[ "_children" ][time][ "_result_ptr" ]); 
//                 MP list_visualisation_output = output_field["visualization.color_by.lst"];
//             
//                 list_visualisation.clear();
//                 list_visualisation << list_visualisation_output;
// 
//                 mp[ "_old_time_step" ] = time;
//             }             
//         }
        // ----------------------------------------------------
       
       
       
    }
}
