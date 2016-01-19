/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/csma-module.h>
#include <ns3/internet-module.h>
#include <ns3/point-to-point-module.h>
#include <ns3/applications-module.h>
#include <ns3/ipv4-global-routing-helper.h>
#include <ns3/onoff-application.h>
#include <ns3/gnuplot.h>
#include <string.h>
#include <math.h>
 
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Topologia");

// Se tienen numClientes dispositivos que actúan como clientes y numServidores dispostivos
// que actúan como servidores. Todos ellos se conectan a un dispositivo central (Engine) que
// hace las tareas de router + switch + balanceador.
// En la implementación final: Engine+Servidores misma red y clientes en internet

// Topología
//		10.1.1.0/24
//                clientes
//                   |
//       ------------------------
//       |        ENGINE        |
//       ------------------------
//        |      |      |      |
//      serv0  serv1   ...  servN
//             10.1.1.254/24
//

// En esta primera versión está todo desarrollado en una red local, es decir, clientes, servidores y Engine se encuentran
// en la misma red, por eso no se ha usado IP para encaminar (Engine = switch)

// To Do: 
//	- Usar enrutamiento IP
//	- Jerarquizar clientes mediantes más switchs fuera de la red local
//	- Mecanismos de balanceo de carga (idea, usar redes definidas por software (SDN)) por:
//		- Más configurables
//		- Soportan varios mecanismos
//		- Tengo idea de cómo hacerlo

int main (int argc, char *argv[]) {

	// Se crean los nodos 
	// Clientes
	NS_LOG_INFO ("Creando clientes...");
	NodeContainer clientes;
	clientes.Create(numClientes);

	// Servidores
	NS_LOG_INFO ("Creando servidores...");
	NodeContainer servidores;
	servidores.Create(numServidores);

	// Engine
	NS_LOG_INFO ("Creando Engine...");
	NodeContainer csmaEngine;
	csmaEngine.Create(1);

	// Características del canal
	CsmaHelper csma;
	csma.SetChannelAttribute("DataRate", DataRateValue(capacidad));
	csma.SetChannelAttribute("Delay", TimeValue(retardoProp));

	// Creamos los enlaces entre los equipos y el Engine
	NetDeviceContainer clienteDev;
	NetDeviceContainer servidorDev;
	NetDeviceContainer engineDev;
	// Servidores
	for (int i = 0; i < numServidores; i++)
	{
		NetDeviceContainer link = csma.Install(NodeContainer(servidores.Get(i), csmaEngine));
		servidorDev.Add(link.Get(0));	// Extremo equipo
		engineDev.Add(link.Get(1));	// Extremo engine
	}
	// Clientes
	for (int i = 0; i < numClientes; i++)
	{
		NetDeviceContainer link = csma.Install(NodeContainer(clientes.Get(i), csmaEngine));
		clienteDev.Add(link.Get(0));	// Extremo equipo
		engineDev.Add(link.Get(1));	// Extremo engine
	}

  	// Añadimos la pila de internet a los equipos
	InternetStackHelper internet;
	internet.Install(servidores);
	internet.Install(clientes);

  	// Asigna dirección IP a los servidores
	NS_LOG_INFO ("Asignando dirección IP a los servidores...");
	for (int i = 0; i < numServidores; i++) {
		Ptr<NetDevice> device = servidorDev.Get(i);		// device = servidor i 
		Ptr<Node> node = device->GetNode();			// node = nodo del servidor i
		Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();		// ipv4 = objeto IP del nodo del servidor i

		int32_t interface = ipv4->GetInterfaceForDevice(device); 	// interface = interfaz del servidor i
		// Si no tiene interfaz, se le añade
		if (interface == -1) {
			interface = ipv4->AddInterface(device);
		}
		// Todos los servidores con la misma IP/máscara, puesto que el balanceo no se hará por IP
		Ipv4InterfaceAddress ipv4Addr = Ipv4InterfaceAddress("10.1.1.254", "255.255.255.0");
		ipv4->AddAddress(interface, ipv4Addr);
		ipv4->SetMetric(interface, 1);
		ipv4->SetUp(interface);
	}

  	// Asigna dirección IP a los clientes
	NS_LOG_INFO ("Asignando dirección IP a los clientes...");
	Ipv4AddressHelper ipv4;
	ipv4.SetBase("10.1.1.0", "255.255.255.0");
	ipv4.Assign(clienteDev);

	uint16_t port= 9;   // Puerto para la conexión

  	// Creamos aplicación para los clientes
	NS_LOG_INFO ("Creando aplicación para los clientes...");

	// Aplicación on/off
  	OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address("10.1.1.254"), port)));
  	onoff.SetConstantRate(DataRate("500kb/s"));

  	ApplicationContainer app;	// Contenedor para las aplicaciones cliente y servidor

  	// Instala la aplicación en todos los clientes
  	for (int i = 0; i < numClientes; i++) {
  		app = onoff.Install(clientes.Get(i));
  		app.Start(Seconds(1.0));
  		app.Stop(Seconds(10.0));
  	}

  	// Creamos sumideros para los servidores
  	NS_LOG_INFO ("Creando sumidero para los servidores...");
  	
  	// Sumidero 
  	PacketSinkHelper sink("ns3::UdpSocketFactory",Address(InetSocketAddress(Ipv4Address::GetAny (), port)));
  	// Instala el sumidero en todos los servidores
  	for (int i = 0; i < numServidores; i++) {
  		app = sink.Install(servidores.Get(i));
  	}
}
