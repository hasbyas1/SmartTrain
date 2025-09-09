import React from "react";

export const Header = (props) => {
  return (
    <header id="header" className="relative bg-gray-800 text-white">
      <div className="intro relative">
        <div className="overlay bg-black/50">
          <div className="container mx-auto px-4 py-32 flex justify-center items-center">
            <div className="text-center !md:max-w-xl pt-48">
              <h1 className="!text-7xl !md:text-5xl font-bold mb-4">
                {props.data ? props.data.title : "Loading"}
              </h1>
              <p className="!text-2xl max-w-3xl py-12 text-gray-200">
                {props.data ? props.data.paragraph : "Loading"}
              </p>
            </div>
          </div>
        </div>
      </div>
    </header>
  );
};
