node {
  stage('Checkout') {
    checkout scm
  }
  stage('Build') {
    echo 'Hello from Jenkins!'
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
