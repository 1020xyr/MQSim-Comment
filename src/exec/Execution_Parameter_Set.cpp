#include "Execution_Parameter_Set.h"


Host_Parameter_Set Execution_Parameter_Set::Host_Configuration;
Device_Parameter_Set Execution_Parameter_Set::SSD_Device_Configuration;


void Execution_Parameter_Set::XML_serialize(Utils::XmlWriter& xmlwriter)
{
	// 未指定配置文件时调用的方法，没必要仔细看  XML_serialize方法都没必要看
	std::string tmp;
	tmp = "Execution_Parameter_Set";
	xmlwriter.Write_open_tag(tmp);

	Host_Configuration.XML_serialize(xmlwriter);
	SSD_Device_Configuration.XML_serialize(xmlwriter);

	xmlwriter.Write_close_tag();
}

void Execution_Parameter_Set::XML_deserialize(rapidxml::xml_node<> *node)
{
	try {
		for (auto param = node->first_node(); param; param = param->next_sibling()) {
			// 分别给主机配置类和SSD设备配置类赋值
			if (strcmp(param->name(), "Host_Parameter_Set") == 0) {
				Host_Configuration.XML_deserialize(param);
			} else if (strcmp(param->name(), "Device_Parameter_Set") == 0) {
				SSD_Device_Configuration.XML_deserialize(param);
			}
		}
	} catch (...) {
		PRINT_ERROR("Error in the Execution_Parameter_Set!")
	}
}
