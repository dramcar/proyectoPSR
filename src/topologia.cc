/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <string.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include <ns3/applications-module.h>
#include <ns3/ipv4-global-routing-helper.h>
#include <ns3/onoff-application.h>
#include <ns3/gnuplot.h>
#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/csma-module.h>
#include <ns3/internet-module.h>
#include <ns3/applications-module.h>
#include <ns3/openflow-module.h>
#include <ns3/log.h>

#include "openflow-loadbalancer.h"
#include "openflow-controller.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Topologia");

// Se tienen client_number dispositivos que actúan como clientes y server_number dispostivos
// que actúan como servidores. Todos ellos se conectan a un switch, el cual está asociado
// a un dispositivo controlador, el cual mediante características de redes Definidas por
// Software y de tráfico mediante OpenFlow es capaz de gestionar el comportamiento del switch.
// En la implementación final: switch+servidores en la misma red y clientes en internet

// Topología
//				10.1.1.0/24
//                clientes
//                   |
//       ------------------------		 	  ---------------
//       |        switch        |<----------->|	Controller	|
//       ------------------------		      ---------------
//        |      |      |      |
//      serv0  serv1   ...  servN
//             10.1.1.254/24
//

// En esta primera versión está todo desarrollado en una red local, es decir, clientes, servidores y sitch se encuentran
// en la misma red, por eso no se ha usado IP para encaminar

// To Do: 
//	- Usar enrutamiento IP
//	- Jerarquizar clientes mediantes más switchs fuera de la red local
//	- Mecanismos de balanceo de carga por:
//		- Más configurables
//		- Soportan varios mecanismos
//		- Tengo idea de cómo hacerlo
// 	- Parámetros por línea de comando en el lanzamiento:
//		- Tipo de balanceo
//		- Atributos enlaces
//		- Nº Servidores
//		- ...

int main (int argc, char *argv[]) {

	// Se crean los nodos 
	// Clientes
	NS_LOG_INFO ("Creando clientes...");
	NodeContainer clients;
	clients.Create(client_number);

	// Servidores
	NS_LOG_INFO ("Creando servidores...");
	NodeContainer servers;
	servers.Create(server_number);

	// Engine
	NS_LOG_INFO ("Creando switch...");
	NodeContainer csmaSwitch;
	csmaSwitch.Create(1);

	// Características del canal
	CsmaHelper csma;
	csma.SetChannelAttribute("DataRate", DataRateValue(data_rate));
	csma.SetChannelAttribute("Delay", TimeValue(delay));

	// Creamos los enlaces entre los equipos y el Engine
	NetDeviceContainer clientDevices;
	NetDeviceContainer serverDevices;
	NetDeviceContainer switchDevices;
	// Servidores
	for (int i = 0; i < server_number; i++)
	{
		NetDeviceContainer link = csma.Install(NodeContainer(servers.Get(i), csmaSwitch));
    	serverDevices.Add(link.Get(0));		// Extremo servidor
    	switchDevices.Add(link.Get(1));		// Extremo switch
    }
	// Clientes
    for (int i = 0; i < client_number; i++)
    {
    	NetDeviceContainer link = csma.Install(NodeContainer(clients.Get(i), csmaSwitch));
    	clientDevices.Add(link.Get(0));		// Extremo cliente
    	switchDevices.Add(link.Get(1));		// Extremo switch
    }

    // Creamos el netdevice switch con OpenFlow, el cual realizará las tareas de reenvío 
    Ptr<Node> switchNode = csmaSwitch.Get(0);
    OpenFlowSwitchHelper swtch;

  	// Creamos el controlador vacío, en función del modo elegido se asignará posteriormente
  	// una u otra implementación
    Ptr<ns3::ofi::Controller> controller = NULL;

    switch (lb_type) {
    	// Modo aleatorio
    	case RANDOM: {
    		NS_LOG_INFO("RANDOM (Usando balanceo de carga aleatorio)");
    		// Ahora si creamos el controlador de este tipo
    		controller = CreateObject<ns3::ofi::RandomizeController>();
    		// Instala switch:
    		//		- Añade el dispositivo swtch al nodo switchNode
    		//		- Añade los equipos switchDevices como puertos del switch
    		//		- Establece el controlador para este modo de balanceo
    		swtch.Install(switchNode, switchDevices, controller);
    		break;
    	}
    	// Modo Round Robin
    	case ROUND_ROBIN: {
    		NS_LOG_INFO("ROUND_ROBIN (Usando balanceo de carga round robin)");
    		// Ahora si creamos el controlador de este tipo
    		controller = CreateObject<ns3::ofi::RoundRobinController>();
    		// Instala switch:
    		//		- Añade el dispositivo swtch al nodo switchNode
    		//		- Añade los equipos switchDevices como puertos del switch
    		//		- Establece el controlador para este modo de balanceo
    		swtch.Install(switchNode, switchDevices, controller);
    		break;
    	}
    	// Modo basado en IP hash
    	case IP_HASHING: {
    		NS_LOG_INFO("IP-HASH (Usando balanceo de carga IP hash)");
    		// Ahora si creamos el controlador de este tipo
    		controller = CreateObject<ns3::ofi::IPHashingController>();
    		// Instala switch:
    		//		- Añade el dispositivo swtch al nodo switchNode
    		//		- Añade los equipos switchDevices como puertos del switch
    		//		- Establece el controlador para este modo de balanceo
    		swtch.Install(switchNode, switchDevices, controller);
    		break;
    	}
    	default:{
    		// No tipo de balanceo, no party
    		break;
    	}
    }

  	// Añadimos la pila de internet a los equipos
    InternetStackHelper internet;
    internet.Install(servers);
    internet.Install(clients);

  	// Asigna dirección IP a los servidores
    NS_LOG_INFO ("Asignando dirección IP a los servidores...");
    for (int i = 0; i < server_number; i++) {
		Ptr<NetDevice> device = serverDevices.Get(i);	// device = servidor i 
		Ptr<Node> node = device->GetNode();				// node = nodo del servidor i
		Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();		// ipv4 = objeto IP del nodo del servidor i

		int32_t interface = ipv4->GetInterfaceForDevice(device); 	// interface = interfaz del servidor i
		// Si no tiene interfaz, se le asigna
		if (interface == -1) {
			interface = ipv4->AddInterface(device);
		}
		// Todos los servidores con una única IP/máscara (10.1.1.254/24)
		// De cara al exterior no se dan detalles del tipo de balanceo, nº servidores, etc; se accede a una IP
		// e internamente se realiza el balanceo de forma transparente al usuario
		Ipv4InterfaceAddress ipv4Addr = Ipv4InterfaceAddress("10.1.1.254", "255.255.255.0");
		ipv4->AddAddress(interface, ipv4Addr);
		ipv4->SetMetric(interface, 1);
		ipv4->SetUp(interface);
	}

  	// Asigna dirección IP a los clientes
	NS_LOG_INFO ("Asignando dirección IP a los clientes...");
	// Todos los clientes con IPs de la subred 10.1.1.0
	Ipv4AddressHelper ipv4;
	ipv4.SetBase("10.1.1.0", "255.255.255.0");
	ipv4.Assign(clientDevices);

	uint16_t port= 9;   // Puerto para la conexión
  	ApplicationContainer app;	// Contenedor para las aplicaciones de cliente y servidor

  	// Creamos aplicación para los clientes
	NS_LOG_INFO ("Creando aplicación para los clientes...");

	// Aplicación on/off
	OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address("10.1.1.254"), port)));
	onoff.SetConstantRate(DataRate("500kb/s"));

  	// Instala la aplicación en todos los clientes
  	for (int i = 0; i < client_number; i++) {
  		app = onoff.Install(clients.Get(i));
  		app.Start(Seconds(1.0));
  		app.Stop(Seconds(10.0));
  	}

  	// Creamos sumideros para los servidores
  	NS_LOG_INFO ("Creando sumidero para los servidores...");
  	
  	// Sumidero 
  	PacketSinkHelper sink("ns3::UdpSocketFactory",Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
  	// Instala el sumidero en todos los servidores
  	for (int i = 0; i < server_number; i++) {
  		app = sink.Install(servers.Get(i));
  		app.Start(Seconds (0.0));
  	}

  	NS_LOG_INFO ("Lanzando simulación");
  	Simulator::Run();
  	Simulator::Destroy();
  	NS_LOG_INFO ("Hecho");
  }