#include <ctype.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <stdio.h>
#include <time.h>
#include <map>

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/aodv-helper.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("P3");

//call back method for total transfer byte count
int totalTxPacketCounter = 0;
static void TxCount(Ptr <const Packet> p){
        totalTxPacketCounter++;
}

int main(int argc, char *argv[]){

        LogComponentEnable("P3", LOG_LEVEL_INFO);
        std::string phyMode("OfdmRate6Mbps");
        std::string size="1000";
        //bandwidth
        unsigned int BW = 54000000;
        //protocol
        std::string routingProtocol = "AODV";
        double packetSize = 512;
        int nodeNumber = 50;
        double power = 16;
        double intensity = 0.9;
        int rate;

        CommandLine cmd;
        cmd.AddValue("nodeNumber", "intput number of Nodes", nodeNumber);
        cmd.AddValue("power", "input Power in Transmission", power);
        cmd.AddValue("intensity", "input Intensity of Traffic", intensity);
        cmd.AddValue ("routingProtocol", "Set routing protocol to AODV or OLSR", routingProtocol);
        cmd.Parse (argc, argv);


        rate = (intensity * (double)BW) * 2 / nodeNumber;


        //set nodes as wifi nodes
         NodeContainer wifiNodes;
         wifiNodes.Create(nodeNumber);
        double dataRate= rate;
        //set wifi Phy transmission power range
        Config::SetDefault("ns3::YansWifiPhy::TxPowerEnd",DoubleValue(power));
        Config::SetDefault("ns3::YansWifiPhy::TxPowerStart",DoubleValue(power));

        //fix non-unicast data rate to be the same as that of unicast
        Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue (phyMode));


        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiHelper wifi = WifiHelper::Default();


        NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode",StringValue (phyMode),
                                      "ControlMode",StringValue (phyMode));

        wifiMac.SetType ("ns3::AdhocWifiMac");
        NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, wifiNodes);

        InternetStackHelper stack;
        if( routingProtocol == "AODV" ){
          AodvHelper aodv;
            stack.SetRoutingHelper(aodv);
        } else {
            OlsrHelper olsr;
            stack.SetRoutingHelper(olsr);
        }
        stack.Install (wifiNodes);

        //scatter nodes into required square region randomly
        MobilityHelper mobility;
       std::string randPos = "ns3::UniformRandomVariable[Min=0.0|Max=" + size + "]";
        mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue (randPos),
                                 "Y", StringValue (randPos));
        mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiNodes);

        Ipv4AddressHelper address;
        address.SetBase ("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interface;

        interface = address.Assign(devices);

        uint16_t sinkPort = 9;
        ApplicationContainer sourceApps;
        ApplicationContainer sinkApps;

        for(int i=0;i<nodeNumber;i++)
        {
            PacketSinkHelper udpsink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (),sinkPort));
            sinkApps.Add(udpsink.Install (wifiNodes.Get(i)));
        }

        sinkApps.Start (Seconds (0.0));
        sinkApps.Stop  (Seconds (20.0));

        for(int j=0;j<nodeNumber;j++)
        {
                int dest=rand()%nodeNumber;
                while(dest == j)
                    dest=rand()%nodeNumber;
                OnOffHelper onOffHelper("ns3::UdpSocketFactory", Address(InetSocketAddress(interface.GetAddress(dest), sinkPort)));
                onOffHelper.SetAttribute("OnTime", StringValue("ns3::ExponentialRandomVariable[Mean=0.5]"));
                onOffHelper.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=0.5]"));
                onOffHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
                onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));

                sourceApps.Add(onOffHelper.Install (wifiNodes.Get(j)));
        }
        for (int j = 0; j < nodeNumber; j++){
                sourceApps.Get(j)->TraceConnectWithoutContext ("Tx", MakeCallback (&TxCount));
        }
        sourceApps.Start (Seconds (0.0));
        sourceApps.Stop (Seconds (20.0));

        Simulator::Stop(Seconds(20.0));
        Simulator::Run();

        uint32_t totalRxBytesCounter = 0;

        for (uint32_t i = 0; i < sinkApps.GetN (); i++)
        {
            Ptr <Application> app = sinkApps.Get (i);
            Ptr <PacketSink> pktSink = DynamicCast <PacketSink> (app);
            totalRxBytesCounter += pktSink->GetTotalRx ();
        }
        int totalTxBytesCounter = totalTxPacketCounter * packetSize;
        double efficiency = (double)totalRxBytesCounter / (double)totalTxBytesCounter;
        std::cout << "Protocol: " << routingProtocol << std::endl;
        std::cout <<"Node number: " << nodeNumber << std::endl;
        std::cout << "Power: " << power << " dBm" << std::endl;
        std::cout << "Intensity: " << intensity << std::endl;
        std::cout << "TotalReceive: " << totalRxBytesCounter << " Bytes" << std::endl;
        std::cout << "TotalTransfer: " << totalTxBytesCounter << " Bytes" << std::endl;
        std::cout << "Efficiency: " << efficiency << std::endl;

        Simulator::Destroy ();
        return 0;
        }


                                                                                      174,0-1       Bot
