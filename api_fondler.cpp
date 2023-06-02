#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm> //used for string manipulation, parsing
#include "nlohmann/json.hpp"
#include "curl/curl.h"


using json = nlohmann::json;

// Callback function for cURL to write response data into a string
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
	size_t totalSize = size * nmemb;
	response->append(std::move(std::string(static_cast<char*>(contents), totalSize)));
	return totalSize;
}

// JSON class for the request data
class ModelPrompt {
public:
	std::vector<std::string> modelPrompt;

	// Conversion function from ModelPrompt to json
	operator json() const {
		return json{ {"modelPrompt", modelPrompt} };
	}
};

// Function to send a POST request to the API endpoint and process the response
std::string sendPostRequest(const std::string& url, const std::string& requestData) {
	std::string response;

	try {
		// Initialize cURL
		CURL* curl = curl_easy_init();
		if (curl) {
			// Set the URL
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

			// Set the request data
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestData.data()); // Pass data pointer directly

			// Set the write callback function to handle the response
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

			// Set the content type header
			struct curl_slist* headers = NULL;
			headers = curl_slist_append(headers, "Content-Type: application/json");
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

			// Perform the request
			CURLcode res = curl_easy_perform(curl);

			// Check for errors
			if (res != CURLE_OK) {
				throw std::runtime_error(curl_easy_strerror(res));
			}

			// Clean up
			curl_easy_cleanup(curl);

			// Free the headers list
			curl_slist_free_all(headers);
		}
		else {
			throw std::runtime_error("cURL initialization failed.");
		}
	}
	catch (const std::exception& e) {
		std::cerr << "Error during cURL request: " << e.what() << std::endl;
	}

	return response;
}

// Function to check if the API endpoint is available
bool isApiAvailable(const std::string& apiUrl) {
	std::string response;

	// Initialize cURL
	CURL* curl = curl_easy_init();
	if (curl) {
		// Set the URL
		curl_easy_setopt(curl, CURLOPT_URL, apiUrl.c_str());

		// Set the write callback function to handle the response
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

		// Perform the request
		CURLcode res = curl_easy_perform(curl);

		// Clean up
		curl_easy_cleanup(curl);

		// Check for errors
		if (res != CURLE_OK) {
			std::cerr << "isApiAvailable: Error occurred while checking API availability at " << apiUrl
				<< ", check the docker container is running and has logged 'Application startup complete' to the container terminal: " << curl_easy_strerror(res) << std::endl;
			return false;
		}

		// Parse the response
		json responseData;
		try {
			responseData = json::parse(response);
			//DEBUG
			std::cout << "API_Info: API Response: " << responseData << std::endl << std::endl;

		}
		catch (const json::parse_error& e) {
			std::cerr << "isApiAvailable: Error parsing API response: " << e.what() << std::endl;
			return false;
		}

		// Check if the state field is true
		if (responseData.contains("model_state")) {
			return responseData["model_state"].get<bool>();
		}
		else {
			std::cerr << "isApiAvailable: API is available, but is missing required model state=True." << std::endl;
			return false;
		}
	}
	else {
		std::cerr << "isApiAvailable: cURL initialization failed." << std::endl;
		return false;
	}
}

void processPrompts(const std::string& inputFile, const std::string& outputFile, const int& batchSize, const std::string& apiUrl) {
	// Read prompts from the input file
	std::ifstream ifs(inputFile);
	if (!ifs.is_open()) {
		std::cerr << "Failed to open input file at: " << inputFile << std::endl;
		throw std::runtime_error("Failed to open input file; check that it exists in the location specified and try again.");
	}

	std::vector<std::string> prompts;
	std::string prompt;
	while (std::getline(ifs, prompt)) {
		// Replace tab characters with spaces in the prompt string
		std::replace(prompt.begin(), prompt.end(), '\t', ' ');
		prompts.push_back(prompt);
	}
	ifs.close(); // Close the input file stream

	// Open the output file in table format
	std::ofstream ofs(outputFile);
	if (!ofs.is_open()) {
		std::cerr << "Failed to open output file at: " << outputFile << std::endl;
		throw std::runtime_error("Failed to open output file; check the path exists and that the file is not currently locked for editing.");
	}

	ofs << "Batch\tPrompt\tResponse->\n"; // Output file header row

	// Process prompts in batches of requested batchSize
	for (int i = 0; i < prompts.size(); i += batchSize) {
		int endIdx = min(i + batchSize, static_cast<int>(prompts.size()));
		std::vector<std::string> promptBatch(prompts.begin() + i, prompts.begin() + endIdx);

		// Prepare the request data
		ModelPrompt requestData;
		requestData.modelPrompt = promptBatch;

		// Convert the JSON object to a string
		std::string requestDataStr = json(requestData).dump();

		//DEBUG
		std::cout << "@@RequestBatch [" << i / batchSize << "]: " << requestDataStr << std::endl;

		// Send the request to the API endpoint
		std::string response = sendPostRequest(apiUrl, requestDataStr);

		// Parse the response to ensure it is in json format
		json responseData;
		try {
			responseData = json::parse(response);
		}
		catch (const json::parse_error& e) {
			ofs << i / batchSize << "\t\tAPI Response JSON Parsing Error: " << e.what() << '\n';
			continue;
		}

		//DEBUG
		std::string responseDataStr = responseData.dump();
		std::cout << "@@BatchResponse [" << i / batchSize << "]: " << responseDataStr << std::endl << std::endl;

		// Handle error response
		if (responseData.contains("detail")) {
			const auto& errors = responseData["detail"];
			ofs << i / batchSize << "\t\tError(s) occurred in Batch:";
			for (const auto& error : errors) {
				const auto& msg = error["msg"];
				const auto& type = error["type"];
				ofs << " Message: " << msg.get<std::string>() << "; ";
				ofs << "Type: " << type.get<std::string>() << ";";
			}
			ofs << '\n';
			continue;

			// Write prompts and responses to the output file
		}
		else if (responseData.is_array()) {
			for (size_t j = 0; j < responseData.size(); ++j) {
				//Paste batch and prompt in first column
				ofs << i / batchSize << '\t' << prompts[i + j] << '\t';

				const auto& promptResponse = responseData[j];
				for (const auto& score : promptResponse) {
					std::string label = score["label"].get<std::string>();
					double scoreValue = score["score"].get<double>();
					ofs << label << ": " << scoreValue << '\t';
				}

				ofs << '\n';
			}
		}
		else {
			ofs << i / batchSize << "\t\tUnrecognised API response format: " << responseDataStr << '\t';
			continue;
		}
	}

	ofs.close(); // Close the output file stream

	std::cout << "API Responses received and written to " << outputFile << std::endl;
}

int main() {
	const std::string inputFile = "data/prompts.txt"; //Input file containing prompts, each prompt seperated by a new line
	const std::string outputFile = "data/responses.txt";
	const std::string apiUrl = "http://localhost:8000";
	const std::string apiEndpoint_response = apiUrl + "/response";

	const int batchSize = 10; //Define the number of prompts to pass to the API per batch requst

	if (isApiAvailable(apiUrl)) {
		processPrompts(inputFile, outputFile, batchSize, apiEndpoint_response);
	}

	std::cout << "Press any key to exit...";
	getchar(); // Wait for user to press a key
	return 0;
}