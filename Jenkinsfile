node {
  //agent {
  //  dockerfile {
  //    dir 'docker'
  //    filename 'Dockerfile-x86_64-ubuntu16.04-dev'
  //    label 'my-test-build'
  //  }
  //}
  stage('Checkout') {
    checkout scm
    //stash 
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
  stage('Build') {
    echo 'Hello from stage Build'
    docker.image('682078287735.dkr.ecr.us-east-2.amazonaws.com/devvio-x86_64-ubuntu18.04-ci:20181215').inside {
      sh 'whoami'
      sh 'gcc --version'
      sh 'pwd'
      sh 'cat /etc/lsb-release'
    }
  }
  stage('Test') {
      //
  }
  stage('Deploy') {
      //
  }
}
