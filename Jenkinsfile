node('thor-build') {
  //agent {
  //  dockerfile {
  //    dir 'docker'
  //    filename 'Dockerfile-x86_64-ubuntu16.04-dev'
  //    label 'my-test-build'
  //  }
  //}
    ws("${env.JOB_NAME}") {
  stage('Checkout') {
    checkout scm
    stash name:'scm', includes:'*' 
  }
  //stage('Build') {
  //  echo 'Hello from Jenkins!'
  //  def dockerfile = 'Dockerfile-x86_64-ubuntu16.04-dev'
  //  dir('docker') {
  //    def customImage = docker.build("my-image:${env.BUILD_ID}", "--entrypoint='' -f ${dockerfile} .")
  //    customImage.inside {
  //      sh 'cat /etc/lsb-release'
  //      sh 'gcc --version'
  //    }
  //  }
  //}
  //stage('Build') {
  //  echo 'Hello from stage Build'
  //  docker.withRegistry('https://682078287735.dkr.ecr.us-east-2.amazonaws.com', '90e14b55-62be-4bb8-946f-91a3d9aedf49') {
  //    docker.image('682078287735.dkr.ecr.us-east-2.amazonaws.com/devvio-x86_64-ubuntu18.04-ci:20181215').inside {
  //      sh 'whoami'
  //      sh 'gcc --version'
  //      sh 'pwd'
  //      sh 'cat /etc/lsb-release'
  //    }
  //  }
  //}
  stage('Build') {
      unstash 'scm'
      echo 'Hello from stage Build'
      sh 'whoami'
      sh 'gcc --version'
      sh 'pwd'
      sh 'ls'
      sh 'cat /etc/lsb-release'
      sh 'mkdir build'
      dir('build') {
        sh 'cmake -DCMAKE_BUILD_TYPE=Debug ../src/ -DCMAKE_INSTALL_PREFIX=/mnt/efs/scratch/devv-core/b-$(date +%s)'
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
