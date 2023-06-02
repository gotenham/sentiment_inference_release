REQUIREMENTS:
1. Locally running Docker container built from sentimentinference_img docker image; the container requires a mounted volume containing the inference models, eg %cd%/models:/code/models
2. This folder requires the following structure as a minimum:
[
api_fondler.exe
models
->{model folders}
data
->prompts.txt
]


SETUP STEPS:
1. If you dont have windows subsystem for linux (WSL) installed already, open PowerShell or Windows Command Prompt in administrator mode by 
	right-clicking and selecting "Run as administrator", execute the command inside the braces-> {wsl --install}
2. Download and install docker desktop https://www.docker.com/products/docker-desktop/, you may need to restart your machine after install.
3. Create a folder somewhere to hold the project files, eg C:\projects\sentimentinference; either download this repo, or clone using git from the
	command line using this command -> {git clone https://github.com/gotenham/sentiment_inference_release.git}
3. In this folder you should place the sentimentinference_img.tar image provided seperatly; it should look like this:
	> sentimentinference_img.tar
	> api_fondler.exe
	> a folder named data containing a text file named prompts.txt
	> a folder named models containing the SequenceClassification model folders you'd like to load
		NOTE: download this one from huggingface -> https://huggingface.co/mrm8488/distilroberta-finetuned-financial-news-sentiment-analysis,
		place the folder you've downloaded into the models folder; check that the pytorch_model.bin file downloads correctly as sometimes it gets missed.
		I've been testing successfully with both soleimanian/financial-roberta-large-sentiment and mrm8488/distilroberta-finetuned-financial-news-sentiment-analysis,
		but i recommend starting with the distil model.
4. With docker desktop running, open PowerShell or Windows Command Prompt and navigate to the folder you created containing the project files, eg {cd C:\projects\sentimentinference}; 
	with the terminal now inside the folder where sentimentinference_img.tar is located, execute the command in the braces to import the image to the docker engine-> {docker load -i sentimentinference_img.tar}
5. Once the image is loaded, in the same terminal run the command in the braces to spin up the container and
	mount the /models folder-> {docker run --name sentimentinference_api -p 8000:8000 -d -v %cd%/models:/code/models -e MODEL_NAME=distilroberta-finetuned-financial-news-sentiment-analysis sentimentinference_img}
	NOTE: the container environment variable MODEL_NAME={} determines which model from the models folder will be loaded
6. In docker desktop you can view the progress of startup by going to 'Containers' and selecting the container 'sentimentinference_api', you should see
	a log entry for 'Application startup complete' once the api is ready

The api_fondler.exe executable can be used to send prompts by batch to the docker container api running the inference model and return the sentiment scores.
The prompts to be processed should be placed in the prompts.txt file inside the data folder at this location. Each prompt on a seperate line, max string 
length per prompt is 512 characters, do not leave any blank lines in the prompts.txt file (I've got error handeling for this but just an FYI)

Processed outputs are saved to data/responses.txt, this .txt file will be created if it doesnt already exist.
The contents of responses.txt is tab (\t) delineated and can be manually copied and pasted into an excel sheet to form a workable table of data.

The current api state and loaded model can be checked on http://localhost:8000/
The default inference api endpoint used by api_fondler.exe is http://localhost:8000/response
Use http://localhost:8000/docs for more information on endpoints
