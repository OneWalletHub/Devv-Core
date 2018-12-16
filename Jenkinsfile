def install_prefix = "/mnt/efs/networks/10-11-1-x/install/jenkins/"
node('thor-build') {
    def job_name = "${env.JOB_NAME}"
    def clean_job_name = job_name.replace("%2F",".")
    def workspace_name = "${clean_job_name}-${env.BUILD_ID}"
    def install_dir = "${install_prefix}/${workspace_name}"
    ws(workspace_name) {
	stage('Checkout') {
	    checkout scm
	    stash name:'scm', includes:'*'
	}
	stage('Build') {
	    unstash 'scm'
	    echo 'Hello from stage Build'
	    sh 'pwd'
	    sh 'ls'
	    sh 'cat /etc/lsb-release'
	    sh 'mkdir build'
	    dir('build') {
		sh "cmake -DCMAKE_BUILD_TYPE=Debug ../src/ -DCMAKE_INSTALL_PREFIX=${install_dir}"
		sh 'make -j4'
	    }
	}
	stage('Test') {
	    unstash 'scm'
	    dir('build') {
		sh 'make test'
	    }
	}
	stage('Deploy') {
	    //
	}
    }
}
