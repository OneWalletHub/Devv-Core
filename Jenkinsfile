node {
  stage('Checkout') {
    checkout scm
  }
  stage('Build') {
    echo 'Hello from Jenkins!'
    docker.build(env.JOB_NAME).inside {
      sh 'script/ci'
    }
    def pwd = sh('echo $PWD')
    sh 'ls -l'
  }
  stage('Test') {
      //
  }
  stage('Deploy') {
      //
  }
}
