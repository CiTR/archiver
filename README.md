archiver
========

CiTR audio archiver written in C and Python for Linux. Tested for Ubuntu 16.04 LTS.

Saves audio plugged into the computer's line in as one second mp3 files, each named as the POSIX timestamp they correspond to. One second files are picked so that mp3 files of any length can be created by concatenating the one second chunks. The bitrate is configurable.

Includes a web interface for downloading the audio specified by a start and end time manually, as well as mod_rewrite rules for querying the server to download files automatically if you run something like DJLand.

Archiver written by rtav and web interface written by Brad Winter.

Setup instructions
------------------

All commands given are for Ubuntu Server 16.04 and assume you have followed each step as written. Adapt the commands to your own distro and preference: **

0. Machine selection - we recommend you pick stable hardware for your archiver. Unless you set up two archivers for redundancy, this machine should never be turned off, especially if used in a setting where it logs your radio station's audio for CRTC compliance or if it is used to generate podcast audio. RAID ready hard drives are also recommended for better reliability.

1. Install Linux. We recommend Ubuntu Server since that's the only distro we've tested. Preferably an LTS edition.

Optional - setup a storage RAID for your archiver machine. The archiver generates about 250GB of audio per year, depending on the recording bitrate. We recommend also configuring your RAID in a setup that allows easy growth. An alternative is having an easily hot-swapable drive that allows your technicians to swap in new hard drives when the existing one gets full. This allows long term storage of your audio and conversion to archival formats. You can configure the archiver to store the audio at any location, although we *highly* recommend a local device is used rather than a location on the network.

2. Install alsa, lame, asoundlib, libapache2-mod-python and LAMP stack (some server distros like ubuntu give you the option to install this when installing the operating system)
<pre><code>sudo apt-get install apache2 php libapache2-mod-php php-mcrypt php-mysql php-cli mysql-server alsa lame asoundlib libapache2-mod-python</code></pre>
And then run sudo mysql_secure_installation to set the root password for your mysql db.

3. Enable the mod_rewrite module and on some systems also mod_python. On Ubuntu and other debian-based systems, simply run <code>sudo a2enmod rewrite && sudo service apache2 restart</code>. On other systems, simply uncomment the module line in httpd.conf. On some systems, you may also have to run <code>a2enmod python</code> (it's enabled by default on Ubuntu).

4. Clone this repo to your /home folder
<pre><code>cd /home && sudo git clone https://github.com/CiTR/archiver.git</code></pre>

5. Make sure your vhosts file is set correctly, especially the rewrite rules. An example is found in the sample folder as "archiver.conf": <pre><code>sudo cp /home/archiver/samples/archiver.conf /etc/apache2/sites-enabled/</code></pre>. The defaults in this file should work automatically provided you haven't deviated from these instructions.

6. Edit the config file located at citrlog/config.ini.sample and save it to config.ini. You will have to define the "audiologbase" line and "logfile" line to a place that works for your setup. Audiologbase is where all your logged audio saves, so pick your folder where your RAID is mounted if you have one.

7. Copy the example cronjob from samples/archive_cron.logger somewhere (such as /etc). If you cloned the repo to /home the defaults will work. Add a cronjob for that script to run on reboot and once every day at 4:00AM (or another unimportant time). Having the script run each day restarts the archive process so that the recording doesn't drift and ensures long-term stability of the machine.
<pre><code>crontab e</code></pre>
Then append these lines onto the bottom:
<pre><code>
@reboot /etc/archive_cron.logger
0 4 * * * /etc/archive_cron.logger
</code></pre>

8. Add your user to the audio group:
<pre><code>sudo usermod -a -G audio <username></code></pre>

9. Reboot

10. Watch for your files be recorded in the directory you specified in step 6. If no audio is being recorded, check that your cronjob ran successfully with <code>ps aux | grep archiver</code>

11. Done!

Compilation Instructions
------------------------

1. Install make, gcc, build-essential, and the libraries for lame, libsndfile.h, and libasound:

sudo apt-get install make gcc build-essential libasound2-dev libmp3lame-dev libsndfile1-dev

2. Navigate to citrlog/src/

3. Run make. You may need to do this with elevated permissions, depending on your user and the location you cloned the repository to.

4. Move the compiled *citrlog* file to the citrlog folder, naming appropiately depending on your architecture.
