#include "StdInc.h"
#include <ClientHttpHandler.h>
#include <ResourceManager.h>
#include <ResourceFilesComponent.h>
#include <ResourceStreamComponent.h>

#include <ServerInstanceBase.h>

static InitFunction initFunction([]()
{
	fx::ServerInstanceBase::OnServerCreate.Connect([](fx::ServerInstanceBase* instance)
	{
		fx::ResourceManager* resman = instance->GetComponent<fx::ResourceManager>().GetRef();

		instance->GetComponent<fx::ClientMethodRegistry>()->AddHandler("getConfiguration", [=](std::map<std::string, std::string>& postMap, const fwRefContainer<net::HttpRequest>& request, const std::function<void(const json&)>& cb)
		{
			json resources = json::array();

			std::string_view filterValues = postMap["resources"];
			std::set<std::string_view> filters;

			if (!filterValues.empty())
			{
				int lastPos = 0;
				int pos = -1;

				do 
				{
					lastPos = pos + 1;
					pos = filterValues.find_first_of(';', pos + 1);

					auto thisValue = filterValues.substr(lastPos, (pos - lastPos));

					filters.insert(thisValue);
				} while (pos != std::string::npos);
			}

			resman->ForAllResources([&](fwRefContainer<fx::Resource> resource)
			{
				if (!filters.empty() && filters.find(resource->GetName()) == filters.end())
				{
					return;
				}

				if (resource->GetState() != fx::ResourceState::Started)
				{
					return;
				}

				json resourceFiles = json::object();
				fwRefContainer<fx::ResourceFilesComponent> files = resource->GetComponent<fx::ResourceFilesComponent>();

				for (const auto& entry : files->GetFileHashPairs())
				{
					resourceFiles[entry.first] = entry.second;
				}

				json resourceStreamFiles = json::object();
				fwRefContainer<fx::ResourceStreamComponent> streamFiles = resource->GetComponent<fx::ResourceStreamComponent>();

				for (const auto& entry : streamFiles->GetStreamingList())
				{
					json obj = json::object({
						{ "hash", entry.second.hashString },
						{ "rscFlags", entry.second.rscFlags },
						{ "rscVersion", entry.second.rscVersion },
						{ "size", entry.second.size },
					});

					if (entry.second.isResource)
					{
						obj["rscPagesVirtual"] = entry.second.rscPagesVirtual;
						obj["rscPagesPhysical"] = entry.second.rscPagesPhysical;
					}

					resourceStreamFiles[entry.first] = obj;
				}

				resources.push_back(json::object({
					{ "name", resource->GetName() },
					{ "files", resourceFiles },
					{ "streamFiles", resourceStreamFiles }
				}));
			});

			cb(json::object({
				{ "fileServer", "http://%s/files" },
				{ "resources", resources }
			}));
		});
	}, 5000);
});
