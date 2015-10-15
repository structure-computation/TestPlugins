#include <QApplication>
#include <QFont>
#include <QFile>

#include <Soca/Com/SodaClient.h>
#include <QtCore/QProcess>
#include <QtCore/QList>
#include <QtCore/QTimer>

#include "config.h"
#include "Launcher.h"


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    SodaClient::_nb_inst = 1;

// ouverture du fichier de configuration du plugin. ce fichier contient l'IP serveur, le port, le conteneur de donnée auquel l'application doit s'inscrire
    config conf;
    conf.read_config();

// préparation de la connexion au serveur et au conteneur de données
    //instanciation du client Soda et du conteneur de données
    QHostAddress adress1(conf.IP);                                  //adresse du serveur
    SodaClient* sc = new SodaClient( adress1, conf.port.toInt() );  //connexion des client Soda avec le port
    sc->reg_type( conf.data_container );                            //connexion aux conteneurs de données observés


// initialisation du launcher d'application à personnaliser
    Launcher *launcher = new Launcher();
    launcher->sc = sc;

// connexion des signaux et slot, action répondant aux événements
    QObject::connect(sc, SIGNAL(new_event(SodaClient::Event)), launcher, SLOT(launch(SodaClient::Event)));    //inscription d'un événement launch lors d'un signal venant de l'objet
    QObject::connect(sc, SIGNAL(quit()), &app, SLOT(quit()));                                                 //fin du programme lorsqu'un serveur s'éteint

// lancement de la boucle d'événement
    return app.exec();

};
