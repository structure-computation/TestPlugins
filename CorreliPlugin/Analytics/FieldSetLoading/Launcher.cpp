#include <iostream>
#include <qglobal.h>
#include "Launcher.h"


Launcher::Launcher(){  
};

Launcher::~Launcher(){
    qDebug() << "CLOSE LOOP";
};

void Launcher::launch(SodaClient::Event event){

    if(event.event_num != 1){  //événement issu d'une modification de l'objet sur le serveur
        MP output_visu = event.mp();

        if(output_visu.type() == "FieldSetCorreliItem"){

            int time = output_visu[ "time.val" ];
            int old_time = output_visu[ "_old_time_step" ];

            if(time != old_time){

                MP data_directory = sc->load_ptr( output_visu[ "_img_field_set_ptr" ]);

                MP output_field = sc->load_ptr(data_directory[ "_children" ][time][ "_result_ptr" ]);

                MP list_visualisation_output = output_field["visualization.color_by.lst"];
                MP list_visualisation = output_visu["visualization.color_by.lst"];

                for(int i=0; i<list_visualisation_output.size(); i++){
                    MP data = list_visualisation_output[i]["data._data"];
                    MP data_visu = list_visualisation[i]["data._data"];
                    for(int j=0; j<data.size(); j++){
                        data_visu.clear();
                        data_visu << data[j];
                    }
                }
                output_visu[ "_old_time_step" ] = time;
            }
            output_visu.flush();
        }
    }
};


