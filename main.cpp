#include <vector>

#include <core.h>
#include <debug.h>

namespace tools {

	/*
	 * Created by ty on 2018-07-11
	 */
	inline std::string trim(std::string s) {
		if (s.empty())
			return s;
		s.erase(0, s.find_first_not_of(' '));
		s.erase(s.find_last_not_of(' ') + 1);
		return s;
	}
}

namespace crawler {

	/*
	 * Created by ty on 2018-07-11
	 */
	inline bool load_seeds(
		const std::string& path, std::vector<url_message>& seeds
	) {
		std::ifstream infile;
		try {
			infile.open(path.data());
			std::string s;
			while (getline(infile, s)) {
				s = tools::trim(s);
				seeds.emplace_back(s);
			}
			infile.close();
			return true;
		}
		catch (...) {
			return false;
		}
	}

	/*
	 * Created by Xie Xiaofei on 2018-07-12
	 * @param src  源文件路径
	 * @param dst  目的文件路径
	 */
	inline bool shuffle(const std::string& src, const std::string& dst) {
		std::ifstream in(src);
		std::ofstream out(dst);
		std::ofstream temp("temp.txt");
		std::pmr::unordered_map<std::string, int> urlMap;
		int sourceID = 0;
		int destID = 0;
		std::string filename;
		std::string line;
		std::string link;
		int count = 0;
		if (in) {
			while (getline(in, line)) {
				std::string sourceUrl;
				std::string destUrl;
				size_t pos = line.find("\t");
				sourceUrl = line.substr(0, pos);
				destUrl = line.substr(pos + 1, line.size());
				//sourceUrl
				std::unordered_map < std::string, int >::iterator sourceIter;
				sourceIter = urlMap.find(sourceUrl);
				if (sourceIter != urlMap.end()) {
					sourceID = sourceIter->second;
				}
				else
				{
					sourceID = count;
					urlMap.insert(std::unordered_map < std::string, int >::value_type(sourceUrl, count));
					out << sourceID << " " << sourceUrl << std::endl;
					count++;
				}

				//destUrl
				std::unordered_map < std::string, int >::iterator destIter;
				destIter = urlMap.find(destUrl);
				if (destIter != urlMap.end()) {
					destID = destIter->second;
				}
				else
				{
					destID = count;
					urlMap.insert(std::unordered_map < std::string, int >::value_type(destUrl, count));
					out << destID << " " << destUrl << std::endl;
					count++;
				}
				temp << sourceID << " " << destID << std::endl;
			}
		}
		else
		{
			return false;
		}
		in.close();
		temp.close();

		out << std::endl;

		std::ifstream tempIn("temp.txt");
		std::set<std::string> pairs;

		while (getline(tempIn, line)) {
			auto itr = pairs.find(line);
			if (pairs.end() != itr) { continue; }
			out << line << std::endl;
			pairs.emplace_hint(itr, std::move(line));
		}

		tempIn.close();
		out.close();

		remove("temp.txt");

		return true;
	}
}

int main(int argc, char** argv) {

	if (3 != argc) {
		tools::log(
			tools::debug_type::FATAL, "main", "Invalid console parameter."
		);
		exit(-1);
	}

	const std::string seeds_file = argv[1];
	const std::string out_file   = argv[2];

	// todo validate the path strings.

	std::vector<crawler::url_message> seeds;

	if (!crawler::load_seeds(seeds_file, seeds) || seeds.empty()) {
		tools::log(
			tools::debug_type::FATAL, "main", "Failed to load seeds."
		);
		exit(-2);
	}

	crawler::core my_crawler(
		seeds.begin(), seeds.end()
	);
	my_crawler.run();

	crawler::shuffle(my_crawler.output_path(), out_file);

	tools::log(
		tools::debug_type::INFO, "main", "Completed, enter a key to quit..."
	);

	std::cin.get();
	return 0;
}